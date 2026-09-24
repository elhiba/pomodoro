#include "YtDlp.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace
{
	const char *const	ReleaseBase = "https://github.com/yt-dlp/yt-dlp/releases/latest/download/";

	// Audio only where the video has an audio-only stream, which every YouTube video and
	// live stream does today. The fallbacks are there so an odd upload still plays,
	// capped in resolution so it does not pull a 4K picture nobody sees.
	const char *const	FormatSelector = "bestaudio/best[height<=480]/best";
}

YtDlp::YtDlp(QObject *parent)
	: QObject(parent)
{
	locate();
}

YtDlp::~YtDlp()
{
	cancel();

	if (_browser)
	{
		_browser->disconnect(this);
		_browser->kill();
		_browser->waitForFinished(1000);
	}

	if (_download)
	{
		_download->abort();
		_downloadFile.remove();
	}
}

bool	YtDlp::available() const
{
	return !_program.isEmpty();
}

bool	YtDlp::installing() const
{
	return _download != nullptr;
}

qreal	YtDlp::installProgress() const
{
	return _installProgress;
}

QString	YtDlp::statusText() const
{
	if (_download)
		return QStringLiteral("Downloading yt-dlp… %1%").arg(qRound(_installProgress * 100));

	if (!_errorText.isEmpty())
		return _errorText;

	if (_program.isEmpty())
		return QStringLiteral("YouTube needs yt-dlp, a free open-source tool. It is not installed yet.");

	return QStringLiteral("Using yt-dlp from %1").arg(QDir::toNativeSeparators(_program));
}

QVariantList	YtDlp::results() const
{
	return _results;
}

bool	YtDlp::searching() const
{
	return _browser != nullptr;
}

QString	YtDlp::resultsError() const
{
	return _resultsError;
}

QString	YtDlp::resultsTitle() const
{
	return _resultsTitle;
}

void	YtDlp::search(const QString &query, bool playlists)
{
	QString	trimmed = query.trimmed();

	if (trimmed.isEmpty())
		return;

	// Videos through yt-dlp's own search; playlists through YouTube's results page with
	// its "playlists only" filter, which yt-dlp reads like any other listing.
	QString	target = playlists
		? QStringLiteral("https://www.youtube.com/results?search_query=%1&sp=EgIQAw%253D%253D")
			.arg(QString::fromLatin1(QUrl::toPercentEncoding(trimmed)))
		: QStringLiteral("ytsearch%1:%2").arg(SearchResults).arg(trimmed);

	browse({ QStringLiteral("--playlist-end"), QString::number(SearchResults), target },
		QString());
}

void	YtDlp::openPlaylist(const QString &url, const QString &title)
{
	browse({ QStringLiteral("--playlist-end"), QString::number(PlaylistEntries), url }, title);
}

void	YtDlp::browse(const QStringList &arguments, const QString &title)
{
	if (_program.isEmpty())
	{
		_resultsError = QStringLiteral("YouTube needs yt-dlp first.");
		emit resultsChanged();
		return;
	}

	if (_browser)
	{
		_browser->disconnect(this);
		_browser->kill();
		_browser->deleteLater();
	}

	_browser = new QProcess(this);
	prepare(_browser);

	_pendingTitle = title;
	_resultsError.clear();

	connect(_browser, &QProcess::finished, this, &YtDlp::onBrowseFinished);

	// --flat-playlist lists entries without opening each one, which is what keeps a
	// search to a few seconds.
	_browser->start(_program, QStringList{
		QStringLiteral("--flat-playlist"),
		QStringLiteral("--dump-single-json"),
		QStringLiteral("--no-warnings")
	} + arguments);

	emit resultsChanged();
}

void	YtDlp::onBrowseFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	QProcess	*process = _browser;

	if (!process)
		return;

	_browser = nullptr;
	process->deleteLater();

	if (exitStatus != QProcess::NormalExit || exitCode != 0)
	{
		_resultsError = QStringLiteral("YouTube did not answer. Try again in a moment.");
		emit resultsChanged();
		return;
	}

	QJsonObject	listing = QJsonDocument::fromJson(process->readAllStandardOutput()).object();
	QVariantList	items;

	for (const QJsonValue &value : listing.value(QStringLiteral("entries")).toArray())
	{
		QJsonObject	entry = value.toObject();
		QString		id = entry.value(QStringLiteral("id")).toString();
		QString		url = entry.value(QStringLiteral("url")).toString();
		bool		playlist = entry.value(QStringLiteral("ie_key")).toString() == QLatin1String("YoutubeTab");

		if (id.isEmpty() || url.isEmpty())
			continue;

		// Videos have a thumbnail at a fixed address, the small one that suits a row;
		// playlists only have whatever the listing carried.
		QString	image;

		if (playlist)
		{
			QJsonArray	thumbnails = entry.value(QStringLiteral("thumbnails")).toArray();

			if (!thumbnails.isEmpty())
				image = thumbnails.last().toObject().value(QStringLiteral("url")).toString();
		}
		else
			image = QStringLiteral("https://i.ytimg.com/vi/%1/mqdefault.jpg").arg(id);

		QString	channel = entry.value(QStringLiteral("channel")).toString();

		if (channel.isEmpty())
			channel = entry.value(QStringLiteral("uploader")).toString();

		bool	live = entry.value(QStringLiteral("live_status")).toString() == QLatin1String("is_live");
		int		duration = entry.value(QStringLiteral("duration")).toInt();
		QString	subtitle = channel;

		if (playlist)
			subtitle = QStringLiteral("Playlist · ") + channel;
		else if (live)
			subtitle += QStringLiteral(" · LIVE");
		else if (duration > 0)
		{
			subtitle += duration >= 3600
				? QStringLiteral(" · %1:%2:%3").arg(duration / 3600).arg(duration / 60 % 60, 2, 10, QLatin1Char('0'))
					.arg(duration % 60, 2, 10, QLatin1Char('0'))
				: QStringLiteral(" · %1:%2").arg(duration / 60).arg(duration % 60, 2, 10, QLatin1Char('0'));
		}

		items.append(QVariantMap{
			{ QStringLiteral("kind"), playlist ? QStringLiteral("playlist") : QStringLiteral("video") },
			{ QStringLiteral("url"), url },
			{ QStringLiteral("title"), entry.value(QStringLiteral("title")).toString() },
			{ QStringLiteral("subtitle"), subtitle },
			{ QStringLiteral("image"), image },
			{ QStringLiteral("live"), live }
		});
	}

	_results = items;
	_resultsTitle = _pendingTitle;

	if (items.isEmpty())
		_resultsError = QStringLiteral("Nothing found.");

	emit resultsChanged();
}

bool	YtDlp::handles(const QUrl &url)
{
	QString	host = url.host().toLower();

	if (host.startsWith(QLatin1String("www.")))
		host = host.mid(4);
	else if (host.startsWith(QLatin1String("m.")))
		host = host.mid(2);

	return host == QLatin1String("youtube.com") || host == QLatin1String("youtu.be")
		|| host == QLatin1String("music.youtube.com");
}

void	YtDlp::resolve(const QUrl &url)
{
	cancel();

	if (_program.isEmpty())
	{
		emit resolveFailed(QStringLiteral("yt-dlp is not installed; get it in Settings → Music"));
		return;
	}

	_resolver = new QProcess(this);
	prepare(_resolver);

	connect(_resolver, &QProcess::finished, this, &YtDlp::onResolveFinished);

	_resolver->start(_program, {
		QStringLiteral("--dump-json"),
		QStringLiteral("--no-playlist"),
		QStringLiteral("--no-warnings"),
		QStringLiteral("--format"), QLatin1String(FormatSelector),
		url.toString()
	});

	// A resolve that hangs (a network that half works) must not leave the player on
	// "Connecting…" for ever; the watchdog upstream would fire first, but this cleans up
	// the process too.
	QTimer::singleShot(ResolveTimeoutMs, _resolver, [this]()
	{
		if (_resolver && _resolver->state() != QProcess::NotRunning)
			_resolver->kill();
	});
}

void	YtDlp::cancel()
{
	if (!_resolver)
		return;

	QProcess	*process = _resolver;

	_resolver = nullptr;

	process->disconnect(this);

	if (process->state() != QProcess::NotRunning)
	{
		process->kill();
		process->waitForFinished(1000);
	}

	process->deleteLater();
}

void	YtDlp::updateIfStale()
{
	// Only a copy this app put there is ours to update; one on PATH belongs to whatever
	// installed it.
	if (_updater || _program != managedPath())
		return;

	QFileInfo	info(_program);

	if (info.lastModified().daysTo(QDateTime::currentDateTime()) < SelfUpdateDays)
		return;

	_updater = new QProcess(this);
	prepare(_updater);

	connect(_updater, &QProcess::finished, this, [this]()
	{
		// -U leaves the file untouched when it is already current, and the age check
		// looks at the modification time, so mark it checked either way.
		QFile	file(_program);

		if (file.open(QIODevice::ReadWrite))
			file.setFileTime(QDateTime::currentDateTime(), QFileDevice::FileModificationTime);

		_updater->deleteLater();
		_updater = nullptr;
	});

	_updater->start(_program, { QStringLiteral("-U") });
}

void	YtDlp::install()
{
	if (_download)
		return;

	_errorText.clear();
	_expectedSha256.clear();
	_installProgress = 0.0;

	// The checksum list first, so the binary can be verified as it arrives.
	QNetworkRequest	request(QUrl(QLatin1String(ReleaseBase) + QStringLiteral("SHA2-256SUMS")));

	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	_download = _network.get(request);

	connect(_download, &QNetworkReply::finished, this, &YtDlp::onSumsFinished);

	emit stateChanged();
	emit installProgressChanged();
}

void	YtDlp::onResolveFinished(int exitCode, QProcess::ExitStatus exitStatus)
{
	QProcess	*process = _resolver;

	if (!process)
		return;

	_resolver = nullptr;
	process->deleteLater();

	QByteArray	output = process->readAllStandardOutput();

	if (exitStatus != QProcess::NormalExit || exitCode != 0)
	{
		// yt-dlp prints "ERROR: [youtube] id: reason"; the reason is what a person can use.
		QString	error = QString::fromUtf8(process->readAllStandardError()).trimmed();
		int		lastLine = error.lastIndexOf(QLatin1Char('\n'));

		if (lastLine >= 0)
			error = error.mid(lastLine + 1);

		int	colon = error.lastIndexOf(QLatin1String(": "));

		if (error.startsWith(QLatin1String("ERROR:")) && colon > 0)
			error = error.mid(colon + 2);

		emit resolveFailed(error.isEmpty() ? QStringLiteral("yt-dlp could not read that link") : error);
		return;
	}

	QJsonObject	info = QJsonDocument::fromJson(output).object();
	QString		stream = info.value(QStringLiteral("url")).toString();

	// A format that yt-dlp would merge from parts carries its URLs per part instead. Only
	// the first can be played here, which with the selector above is the audio.
	if (stream.isEmpty())
	{
		QJsonArray	parts = info.value(QStringLiteral("requested_formats")).toArray();

		if (!parts.isEmpty())
			stream = parts.first().toObject().value(QStringLiteral("url")).toString();
	}

	if (stream.isEmpty())
	{
		emit resolveFailed(QStringLiteral("yt-dlp found no playable audio in that link"));
		return;
	}

	QString	channel = info.value(QStringLiteral("channel")).toString();

	if (channel.isEmpty())
		channel = info.value(QStringLiteral("uploader")).toString();

	emit resolved(QUrl(stream),
		info.value(QStringLiteral("title")).toString(),
		channel,
		info.value(QStringLiteral("is_live")).toBool(),
		info.value(QStringLiteral("thumbnail")).toString());
}

void	YtDlp::onSumsFinished()
{
	QNetworkReply	*reply = _download;

	if (!reply)
		return;

	_download = nullptr;
	reply->deleteLater();

	if (reply->error() != QNetworkReply::NoError)
	{
		failInstall(QStringLiteral("Could not reach GitHub to download yt-dlp"));
		return;
	}

	// Lines of "<sha256>  <file name>".
	const QString	wanted = assetName();
	const QStringList	lines = QString::fromUtf8(reply->readAll()).split(QLatin1Char('\n'));

	for (const QString &line : lines)
	{
		QStringList	fields = line.simplified().split(QLatin1Char(' '));

		if (fields.size() == 2 && fields.at(1) == wanted)
			_expectedSha256 = fields.at(0).toLower();
	}

	if (_expectedSha256.isEmpty())
	{
		failInstall(QStringLiteral("yt-dlp publishes no build for this computer"));
		return;
	}

	QString	path = managedPath() + QStringLiteral(".part");

	QDir().mkpath(QFileInfo(path).absolutePath());

	_downloadFile.setFileName(path);

	if (!_downloadFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		failInstall(QStringLiteral("Could not save yt-dlp: %1").arg(_downloadFile.errorString()));
		return;
	}

	_downloadHash.reset();

	QNetworkRequest	request(QUrl(QLatin1String(ReleaseBase) + wanted));

	request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
		QNetworkRequest::NoLessSafeRedirectPolicy);

	_download = _network.get(request);

	connect(_download, &QNetworkReply::readyRead, this, &YtDlp::onBinaryReadyRead);
	connect(_download, &QNetworkReply::finished, this, &YtDlp::onBinaryFinished);
	connect(_download, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total)
	{
		if (total <= 0)
			return;

		_installProgress = static_cast<qreal>(received) / static_cast<qreal>(total);

		emit installProgressChanged();
		emit stateChanged();
	});

	emit stateChanged();
}

void	YtDlp::onBinaryReadyRead()
{
	if (!_download)
		return;

	QByteArray	chunk = _download->readAll();

	_downloadHash.addData(chunk);

	if (_downloadFile.write(chunk) != chunk.size())
		failInstall(QStringLiteral("Could not save yt-dlp: %1").arg(_downloadFile.errorString()));
}

void	YtDlp::onBinaryFinished()
{
	onBinaryReadyRead();

	QNetworkReply	*reply = _download;

	if (!reply)
		return;

	_download = nullptr;
	reply->deleteLater();

	_downloadFile.close();

	if (reply->error() != QNetworkReply::NoError)
	{
		_downloadFile.remove();
		failInstall(QStringLiteral("The yt-dlp download failed: %1").arg(reply->errorString()));
		return;
	}

	if (QString::fromLatin1(_downloadHash.result().toHex()) != _expectedSha256)
	{
		_downloadFile.remove();
		failInstall(QStringLiteral("The yt-dlp download did not match its published checksum, so it was discarded"));
		return;
	}

	QString	target = managedPath();

	QFile::remove(target);

	if (!_downloadFile.rename(target))
	{
		failInstall(QStringLiteral("Could not put yt-dlp in place"));
		return;
	}

	QFile::setPermissions(target, QFile::permissions(target)
		| QFileDevice::ExeOwner | QFileDevice::ReadOwner);

	_installProgress = 1.0;
	locate();

	emit installProgressChanged();
	emit stateChanged();
}

void	YtDlp::locate()
{
	QString	managed = managedPath();

	if (QFileInfo(managed).isExecutable())
		_program = managed;
	else
		_program = QStandardPaths::findExecutable(QStringLiteral("yt-dlp"));
}

void	YtDlp::failInstall(const QString &reason)
{
	QNetworkReply	*reply = _download;

	_download = nullptr;

	if (reply)
	{
		reply->disconnect(this);
		reply->abort();
		reply->deleteLater();
	}

	if (_downloadFile.isOpen())
	{
		_downloadFile.close();
		_downloadFile.remove();
	}

	_errorText = reason;

	emit stateChanged();
}

void	YtDlp::prepare(QProcess *process) const
{
	process->setProgram(_program);

#ifdef Q_OS_WIN
	// yt-dlp.exe is a console program. Started from a GUI app with the default flags,
	// Windows opens a console window for it that flashes up on every resolve.
	process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *arguments)
	{
		arguments->flags |= CREATE_NO_WINDOW;
	});
#endif
}

QString	YtDlp::assetName()
{
	const QString	cpu = QSysInfo::currentCpuArchitecture();

#if defined(Q_OS_WIN)
	return cpu == QLatin1String("arm64") ? QStringLiteral("yt-dlp_arm64.exe") : QStringLiteral("yt-dlp.exe");
#elif defined(Q_OS_MACOS)
	Q_UNUSED(cpu);
	return QStringLiteral("yt-dlp_macos");
#else
	return cpu == QLatin1String("arm64") ? QStringLiteral("yt-dlp_linux_aarch64") : QStringLiteral("yt-dlp_linux");
#endif
}

QString	YtDlp::managedPath()
{
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
		+ QStringLiteral("/tools");

#ifdef Q_OS_WIN
	return directory + QStringLiteral("/yt-dlp.exe");
#else
	return directory + QStringLiteral("/yt-dlp");
#endif
}
