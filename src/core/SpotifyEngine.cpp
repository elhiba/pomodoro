#include "SpotifyEngine.hpp"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTcpServer>
#include <QUrl>

#include <utility>

#ifdef Q_OS_WIN
#include <windows.h>
#elif defined(Q_OS_LINUX)
#include <csignal>
#include <sys/prctl.h>
#endif

namespace
{
	// A flag the first version kept beside go-librespot's own record; removed on sight.
	const char *const	KeyOldSignedIn = "spotify/playerSignedIn";

	// go-librespot logs the sign-in link as: ... msg="to complete authentication visit
	// the following link: https://accounts.spotify.com/authorize?..."
#ifdef Q_OS_WIN
	// A job that closes every process in it when Pomodoro's last handle to it goes --
	// which the system does for us however Pomodoro ends, crash and Task Manager
	// included. Without it a killed Pomodoro leaves the player running, holding its
	// lock file, and the next one's player cannot start.
	HANDLE	playerJob()
	{
		static HANDLE	job = []()
		{
			HANDLE	created = CreateJobObjectW(nullptr, nullptr);

			if (created)
			{
				JOBOBJECT_EXTENDED_LIMIT_INFORMATION	limits = {};

				limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
				SetInformationJobObject(created, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
			}

			return created;
		}();

		return job;
	}
#endif

	const QRegularExpression	SignInLink(QStringLiteral(R"re(visit the following link: (https://[^\s"]+))re"));
}

SpotifyEngine::SpotifyEngine(QObject *parent)
	: QObject(parent)
{
	_statusTimer.setInterval(StatusPollMs);
	_timeout.setSingleShot(true);

	connect(&_statusTimer, &QTimer::timeout, this, &SpotifyEngine::pollStatus);
	connect(&_timeout, &QTimer::timeout, this, [this]()
	{
		QString	error = _state == SigningIn
			? QStringLiteral("Spotify sign-in timed out. Press Sign in to try again.")
			: QStringLiteral("The Spotify player did not start.");

		stopProcess();
		setState(Failed, error);
	});

	if (!QFile::exists(executablePath()))
		_state = Missing;

	QSettings().remove(QLatin1String(KeyOldSignedIn));

	_remembered = available() && readRemembered();
}

SpotifyEngine::~SpotifyEngine()
{
	stopProcess();
}

SpotifyEngine::State	SpotifyEngine::state() const
{
	return _state;
}

bool	SpotifyEngine::available() const
{
	return _state != Missing;
}

bool	SpotifyEngine::ready() const
{
	return _state == Ready;
}

bool	SpotifyEngine::remembered() const
{
	return _remembered;
}

QString	SpotifyEngine::username() const
{
	return _username;
}

QString	SpotifyEngine::deviceId() const
{
	return _deviceId;
}

QString	SpotifyEngine::errorText() const
{
	return _errorText;
}

const QString	SpotifyEngine::PremiumRequiredText = QStringLiteral(
	"Spotify only plays in other apps for Premium accounts, so this account cannot play "
	"here. Spotify Free works only in Spotify's own app. YouTube and the radio in this panel "
	"work for everyone.");

bool	SpotifyEngine::premiumRequired() const
{
	return _premiumRequired;
}

// In a folder of its own beside the executable, with the decoding libraries it needs.
// Not in the executable's folder: those libraries share file names with the ones Qt's
// FFmpeg brings, built against a different C runtime, and Windows would hand the
// player whichever it found first.
QString	SpotifyEngine::executablePath()
{
#ifdef Q_OS_WIN
	const QString	name = QStringLiteral("go-librespot.exe");
#else
	const QString	name = QStringLiteral("go-librespot");
#endif

	return QCoreApplication::applicationDirPath() + QStringLiteral("/spotify/") + name;
}

void	SpotifyEngine::signIn()
{
	if (!available())
		return;

	_openSignIn = true;

	// Already waiting on a link: open it again, the first tab may have been closed.
	if (_state == SigningIn && !_signInUrl.isEmpty())
	{
		QDesktopServices::openUrl(QUrl(_signInUrl));
		_openSignIn = false;
		return;
	}

	if (_state == Starting || _state == Ready)
		return;

	launch();
}

void	SpotifyEngine::signOut()
{
	stopProcess();

	// state.json holds the stored credentials; without it the next start asks again.
	QFile::remove(configDir() + QStringLiteral("/state.json"));
	_remembered = false;

	_username.clear();
	_deviceId.clear();
	_signInUrl.clear();

	// The next account may well be a Premium one.
	_premiumRequired = false;
	_refusedInARow = 0;

	setState(available() ? Stopped : Missing);
	release();
}

void	SpotifyEngine::start()
{
	if (!remembered() || _state == Starting || _state == SigningIn || _state == Ready)
		return;

	launch();
}

void	SpotifyEngine::whenReady(std::function<void()> then)
{
	if (_state == Ready || _state == Missing)
	{
		then();
		return;
	}

	// Waiting on the user in a browser tab can take minutes; whatever asked is told now
	// that the player is not ready, rather than hanging on it.
	if (_state == SigningIn || ((_state == Stopped || _state == Failed) && !_remembered))
	{
		then();
		return;
	}

	_waiting.append(std::move(then));

	if (_state == Stopped || _state == Failed)
		launch();
}

void	SpotifyEngine::call(const QByteArray &verb, const QString &path, const QJsonObject &body, Reply reply)
{
	if (_port == 0 || !_process)
	{
		reply(0, QJsonObject());
		return;
	}

	QNetworkRequest	request(QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(_port).arg(path)));
	QByteArray		payload;

	if (!body.isEmpty())
	{
		payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
		request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	}

	QNetworkReply	*answer = verb == "GET"
		? _network.get(request)
		: _network.sendCustomRequest(request, verb, payload);

	connect(answer, &QNetworkReply::finished, this, [answer, reply, verb, path]()
	{
		answer->deleteLater();

		int	status = answer->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();

		if (status >= 400)
			qInfo("pomodoro: spotify player %s %s -> %d", verb.constData(), qPrintable(path), status);

		reply(status, QJsonDocument::fromJson(answer->readAll()).object());
	});
}

bool	SpotifyEngine::readRemembered() const
{
	QFile	file(configDir() + QStringLiteral("/state.json"));

	if (!file.open(QIODevice::ReadOnly))
		return false;

	QJsonObject	credentials = QJsonDocument::fromJson(file.readAll()).object()
		.value(QStringLiteral("credentials")).toObject();

	return !credentials.value(QStringLiteral("data")).toString().isEmpty();
}

QString	SpotifyEngine::configDir() const
{
	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/spotify-player");
}

void	SpotifyEngine::launch()
{
	stopProcess();

	_port = freePort();

	if (_port == 0 || !QDir().mkpath(configDir()))
	{
		setState(Failed, QStringLiteral("The Spotify player could not be set up."));
		release();
		return;
	}

	writeConfig();

	_signInUrl.clear();
	_process = new QProcess(this);
	_process->setProgram(executablePath());
	_process->setArguments({ QStringLiteral("--config_dir=") + QDir::toNativeSeparators(configDir()) });
	_process->setWorkingDirectory(QFileInfo(executablePath()).absolutePath());
	_process->setProcessChannelMode(QProcess::MergedChannels);

#ifdef Q_OS_WIN
	// A console program; without this a black window flashes up beside the timer.
	_process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments)
	{
		arguments->flags |= CREATE_NO_WINDOW;
	});
#endif

#ifdef Q_OS_LINUX
	// The player links against libasound just to start, even when it plays through
	// PulseAudio, and many machines -- school ones where nothing can be installed, a
	// fresh WSL -- have none: it then dies with "libasound.so.2: cannot open shared
	// object file". The release ships one beside it, found first from here.
	QProcessEnvironment	environment = QProcessEnvironment::systemEnvironment();
	QString				libraries = QFileInfo(executablePath()).absolutePath();
	QString				existing = environment.value(QStringLiteral("LD_LIBRARY_PATH"));

	environment.insert(QStringLiteral("LD_LIBRARY_PATH"),
		existing.isEmpty() ? libraries : libraries + QLatin1Char(':') + existing);
	_process->setProcessEnvironment(environment);

	// The same on Linux: the player is told to go when Pomodoro does.
	_process->setChildProcessModifier([]()
	{
		prctl(PR_SET_PDEATHSIG, SIGTERM);
	});
#endif

	connect(_process, &QProcess::readyRead, this, &SpotifyEngine::onOutput);
	connect(_process, &QProcess::finished, this, &SpotifyEngine::onFinished);
	connect(_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error)
	{
		if (error != QProcess::FailedToStart)
			return;

		stopProcess();
		setState(Failed, QStringLiteral("The Spotify player could not start."));
		release();
	});

	setState(Starting);

	_process->start();

#ifdef Q_OS_WIN
	if (HANDLE job = playerJob())
	{
		if (HANDLE handle = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, DWORD(_process->processId())))
		{
			AssignProcessToJobObject(job, handle);
			CloseHandle(handle);
		}
	}
#endif

	_statusTimer.start();
	_timeout.start(StartTimeoutMs);
}

// Rewritten on every start, so a port or option change never meets a stale file. The
// credentials are not in here: go-librespot keeps them in state.json beside it.
void	SpotifyEngine::writeConfig()
{
	QSaveFile	file(configDir() + QStringLiteral("/config.yml"));

	if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
		return;

	QByteArray	config =
		"device_name: Pomodoro\n"
		"device_type: computer\n"
		"zeroconf_enabled: false\n"
		"credentials:\n"
		"  type: interactive\n"
		"server:\n"
		"  enabled: true\n"
		"  address: 127.0.0.1\n"
		"  port: " + QByteArray::number(_port) + "\n"
		"volume_steps: 100\n"
		"metadata:\n"
		"  enabled: true\n"
		"  max_tracks: 500\n"
		"log_level: info\n";

#ifdef Q_OS_LINUX
	// go-librespot defaults to ALSA on Linux, and on a desktop ALSA's "default" device is
	// only wired to the real output when the pulse or pipewire ALSA plugin is installed
	// and configured -- often not, and then Spotify signs in, plays, and nothing is heard.
	// Its PulseAudio backend is a client of its own (no libpulse needed), and PipeWire
	// answers it too, so it is used whenever a PulseAudio server is there to talk to.
	QString	runtime = qEnvironmentVariable("XDG_RUNTIME_DIR");
	bool	pulse = !qEnvironmentVariable("PULSE_SERVER").isEmpty()
		|| (!runtime.isEmpty() && QFile::exists(runtime + QStringLiteral("/pulse/native")));

	config += pulse ? "audio_backend: pulseaudio\n" : "audio_backend: alsa\n";
#endif

	file.write(config);
	file.commit();
}

void	SpotifyEngine::onOutput()
{
	while (_process && _process->canReadLine())
	{
		QString	line = QString::fromUtf8(_process->readLine()).trimmed();

		QRegularExpressionMatch	link = SignInLink.match(line);

		if (link.hasMatch())
		{
			_signInUrl = link.captured(1);
			_timeout.start(SignInTimeoutMs);
			setState(SigningIn);

			if (_openSignIn)
			{
				_openSignIn = false;
				QDesktopServices::openUrl(QUrl(_signInUrl));
			}
			else
			{
				// Started on its own with credentials Spotify no longer accepts. Nobody
				// asked for a browser tab; whatever was waiting to play is told instead.
				_remembered = false;
				_errorText = QStringLiteral("Sign in with Spotify again.");
				release();
			}

			continue;
		}

		// A free account. Either Spotify turns the sign-in itself down...
		if (line.contains(QLatin1String("PremiumAccountRequired")))
		{
			markPremiumRequired();
			continue;
		}

		// ...or it signs in and then refuses the key for every song. One refusal is a song
		// Spotify will not license here; a run of them with nothing played is the account.
		if (line.contains(QLatin1String("refused the audio key")))
		{
			if (++_refusedInARow >= RefusalsForFreeAccount)
				markPremiumRequired();
		}
		else if (line.contains(QLatin1String("msg=\"loaded ")))
			_refusedInARow = 0;

		// Problems are worth a line in the log. Never the whole output: it names the
		// account.
		if (line.contains(QLatin1String("level=error")) || line.contains(QLatin1String("level=fatal")))
		{
			QString	message = line.section(QStringLiteral("msg="), 1);

			qInfo("pomodoro: spotify player: %s", qPrintable(message.left(200)));
		}
	}
}

void	SpotifyEngine::onFinished(int exitCode, QProcess::ExitStatus)
{
	if (!_process)
		return;

	_process->deleteLater();
	_process = nullptr;
	_port = 0;
	_statusTimer.stop();
	_timeout.stop();

	setState(Failed, _premiumRequired
		? PremiumRequiredText
		: QStringLiteral("The Spotify player stopped (code %1).").arg(exitCode));
	release();
}

void	SpotifyEngine::markPremiumRequired()
{
	if (_premiumRequired)
		return;

	_premiumRequired = true;
	_errorText = PremiumRequiredText;

	qInfo("pomodoro: spotify player: the account is not Premium");

	emit stateChanged();
	emit premiumRequiredFound();
}

void	SpotifyEngine::pollStatus()
{
	call("GET", QStringLiteral("/status"), QJsonObject(), [this](int status, const QJsonObject &body)
	{
		// 204 is "no session yet", 0 is "not listening yet": keep waiting either way.
		if (status != 200 || _state == Ready)
			return;

		_username = body.value(QStringLiteral("username")).toString();
		_deviceId = body.value(QStringLiteral("device_id")).toString();

		_statusTimer.stop();
		_timeout.stop();

		_remembered = true;

		setState(Ready);
		release();
	});
}

void	SpotifyEngine::setState(State state, const QString &error)
{
	if (state != Failed)
		_errorText.clear();
	else
		_errorText = error;

	if (_state == state && state != Failed)
		return;

	_state = state;

	emit stateChanged();
}

void	SpotifyEngine::release()
{
	const QList<std::function<void()>>	waiting = std::exchange(_waiting, {});

	for (const std::function<void()> &then : waiting)
		then();
}

void	SpotifyEngine::stopProcess()
{
	_statusTimer.stop();
	_timeout.stop();
	_port = 0;

	if (!_process)
		return;

	QProcess	*process = std::exchange(_process, nullptr);

	process->disconnect(this);
	process->kill();
	process->waitForFinished(2000);
	process->deleteLater();
}

// A port nothing is using right now, found by letting the system pick one. There is a
// moment between closing it and the player opening it, which on a desktop is harmless.
quint16	SpotifyEngine::freePort()
{
	QTcpServer	probe;

	if (!probe.listen(QHostAddress::LocalHost, 0))
		return 0;

	quint16	port = probe.serverPort();

	probe.close();

	return port;
}
