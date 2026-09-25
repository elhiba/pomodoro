#include "DiscordPresence.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QTextBoundaryFinder>
#include <QUuid>
#include <QtEndian>

#ifndef Q_OS_WIN
#include <unistd.h>
#endif

#ifndef POMODORO_DISCORD_CLIENT_ID
#define POMODORO_DISCORD_CLIENT_ID ""
#endif

namespace
{
	// Cuts text to what Discord accepts without breaking a character in half. A plain
	// left() counts UTF-16 units, so it could split an emoji's surrogate pair, or a
	// flag or a family emoji built from several code points, and Discord refuses the
	// whole status for the broken text. Cutting at a grapheme boundary keeps every
	// character whole. The limit is in UTF-16 units: text that fits in those fits
	// however Discord counts characters.
	QString	fitText(const QString &text, int limit)
	{
		if (text.size() <= limit)
			return text;

		QTextBoundaryFinder	finder(QTextBoundaryFinder::Grapheme, text);

		finder.setPosition(limit);

		if (!finder.isAtBoundary())
			finder.toPreviousBoundary();

		return text.left(qMax(0, finder.position())).trimmed();
	}

	const char *const	KeyEnabled = "discord/enabled";

	// Discord takes the first free of ten pipes; a second Discord (PTB, Canary) the next.
	constexpr int	PipeCount = 10;

#ifndef Q_OS_WIN
	// Where a sandboxed Discord puts its socket, under the runtime directory: Flatpak
	// keeps each app's in app/<id>, Snap in snap.<name>, and Vesktop's Flatpak in its own
	// xdg-run. Checked by name first; anything else is found by the search below.
	const char *const	SandboxFolders[] = {
		"",
		"app/com.discordapp.Discord",
		"app/com.discordapp.DiscordCanary",
		"app/com.discordapp.DiscordPTB",
		"app/dev.vencord.Vesktop",
		".flatpak/com.discordapp.Discord/xdg-run",
		".flatpak/dev.vencord.Vesktop/xdg-run",
		"snap.discord",
		"snap.discord-canary",
		"snap.discord-ptb"
	};
#endif
}

DiscordPresence::DiscordPresence(QObject *parent)
	: QObject(parent)
{
	_enabled = QSettings().value(QLatin1String(KeyEnabled), false).toBool();

	_retryTimer.setSingleShot(true);
	_retryTimer.setInterval(RetryMs);

	_sendTimer.setSingleShot(true);

	connect(&_retryTimer, &QTimer::timeout, this, &DiscordPresence::connectToDiscord);
	connect(&_sendTimer, &QTimer::timeout, this, &DiscordPresence::sendActivity);
	connect(&_socket, &QLocalSocket::connected, this, &DiscordPresence::onConnected);
	connect(&_socket, &QLocalSocket::readyRead, this, &DiscordPresence::onReadyRead);
	connect(&_socket, &QLocalSocket::disconnected, this, &DiscordPresence::onDisconnected);
	connect(&_socket, &QLocalSocket::errorOccurred, this, [this]()
	{
		// Not listening on this pipe: try the next, and after the last, wait a while.
		if (_socket.state() == QLocalSocket::UnconnectedState)
			onDisconnected();
	});

	if (_enabled && available())
		QTimer::singleShot(0, this, &DiscordPresence::connectToDiscord);
}

bool	DiscordPresence::enabled() const
{
	return _enabled;
}

void	DiscordPresence::setEnabled(bool enabled)
{
	if (_enabled == enabled)
		return;

	_enabled = enabled;
	QSettings().setValue(QLatin1String(KeyEnabled), enabled);

	if (_enabled)
	{
		_target = 0;
		_targets.clear();
		connectToDiscord();
	}
	else
	{
		// Taken off the profile at once, then the connection closed.
		clearActivity();
		_retryTimer.stop();
		_sendTimer.stop();
		_socket.flush();
		_socket.disconnectFromServer();
	}

	emit stateChanged();
}

bool	DiscordPresence::available() const
{
	return !clientId().isEmpty();
}

bool	DiscordPresence::connected() const
{
	return _ready;
}

QString	DiscordPresence::userName() const
{
	return _userName;
}

QString	DiscordPresence::statusText() const
{
	if (!available())
		return QStringLiteral("Not available in this build");

	if (!_enabled)
		return QStringLiteral("Off");

	if (_ready)
	{
		QString	connected = _userName.isEmpty()
			? QStringLiteral("Connected to Discord")
			: QStringLiteral("Connected as %1").arg(_userName);

		// Connected, but Discord turned the last status down; it says why.
		return _error.isEmpty() ? connected : connected + QStringLiteral(" · ") + _error;
	}

	if (!_error.isEmpty())
		return _error;

	return QStringLiteral("Waiting for Discord to open…");
}

void	DiscordPresence::show(const QVariantMap &activity)
{
	_wanted = activity;

	if (!_ready)
		return;

	// Coalesced: a burst of changes goes out as one update, never faster than Discord
	// allows.
	qint64	since = QDateTime::currentMSecsSinceEpoch() - _lastSentAt;

	if (!_sendTimer.isActive())
		_sendTimer.start(since >= MinimumGapMs ? 300 : int(MinimumGapMs - since));
}

void	DiscordPresence::connectToDiscord()
{
	if (!_enabled || !available() || _socket.state() != QLocalSocket::UnconnectedState)
		return;

	// A fresh look at the start of every round, so a Discord started (or moved into a
	// sandbox) since the last round is found.
	if (_target == 0)
		_targets = findTargets();

	if (_targets.isEmpty())
	{
		if (_error.isEmpty() || _ready)
		{
			_error.clear();
			emit stateChanged();
		}

		_retryTimer.start();
		return;
	}

	_buffer.clear();
	_socket.connectToServer(_targets.at(_target));
}

void	DiscordPresence::onConnected()
{
	write(Handshake, QJsonObject{
		{ QStringLiteral("v"), 1 },
		{ QStringLiteral("client_id"), clientId() }
	});
}

void	DiscordPresence::onReadyRead()
{
	_buffer.append(_socket.readAll());

	// Whole frames only: an 8-byte header, then the JSON it announces.
	while (_buffer.size() >= 8)
	{
		quint32	opcode = qFromLittleEndian<quint32>(_buffer.constData());
		quint32	length = qFromLittleEndian<quint32>(_buffer.constData() + 4);

		if (qsizetype(8 + length) > _buffer.size())
			break;

		QJsonObject	message = QJsonDocument::fromJson(_buffer.mid(8, length)).object();

		_buffer.remove(0, 8 + length);
		handle(int(opcode), message);
	}
}

void	DiscordPresence::onDisconnected()
{
	bool	wasReady = _ready;

	_ready = false;
	_lastSent.clear();

	if (!_enabled)
	{
		if (wasReady)
			emit stateChanged();

		return;
	}

	// Next place straight away; once all are tried, wait and start over.
	if (!wasReady && ++_target < _targets.size())
	{
		QTimer::singleShot(0, this, &DiscordPresence::connectToDiscord);
		return;
	}

	_target = 0;
	_retryTimer.start();

	emit stateChanged();
}

void	DiscordPresence::handle(int opcode, const QJsonObject &message)
{
	switch (opcode)
	{
		case Ping:
			write(Pong, message);
			return;

		case Close:
			// Discord refused us, usually a wrong Application ID; it says why.
			_error = message.value(QStringLiteral("message")).toString();
			_socket.disconnectFromServer();
			emit stateChanged();
			return;

		case Frame:
			break;

		default:
			return;
	}

	QString	event = message.value(QStringLiteral("evt")).toString();

	if (event == QLatin1String("READY"))
	{
		QJsonObject	user = message.value(QStringLiteral("data")).toObject().value(QStringLiteral("user")).toObject();
		QString		shown = user.value(QStringLiteral("global_name")).toString();

		_userName = shown.isEmpty() ? user.value(QStringLiteral("username")).toString() : shown;
		_ready = true;
		_error.clear();
		_lastSentAt = 0;

		emit stateChanged();

		if (!_wanted.isEmpty())
			show(_wanted);
	}
	else if (event == QLatin1String("ERROR"))
	{
		_error = message.value(QStringLiteral("data")).toObject().value(QStringLiteral("message")).toString();
		qInfo("pomodoro: discord: %s", qPrintable(_error));
		emit stateChanged();
	}
	else if (message.value(QStringLiteral("cmd")).toString() == QLatin1String("SET_ACTIVITY") && !_error.isEmpty())
	{
		// A status went through, so an earlier refusal no longer applies.
		_error.clear();
		emit stateChanged();
	}
}

void	DiscordPresence::write(int opcode, const QJsonObject &message)
{
	QByteArray	payload = QJsonDocument(message).toJson(QJsonDocument::Compact);
	QByteArray	frame(8, Qt::Uninitialized);

	qToLittleEndian<quint32>(quint32(opcode), frame.data());
	qToLittleEndian<quint32>(quint32(payload.size()), frame.data() + 4);

	_socket.write(frame + payload);
}

QJsonObject	DiscordPresence::activityJson()
{
	auto	text = [this](const char *key)
	{
		// Discord rejects fields shorter than two characters or longer than 128.
		QString	value = fitText(_wanted.value(QLatin1String(key)).toString().trimmed(), 128);

		return value.size() < 2 ? QString() : value;
	};

	QJsonObject	activity;

	if (!text("details").isEmpty())
		activity.insert(QStringLiteral("details"), text("details"));

	if (!text("state").isEmpty())
		activity.insert(QStringLiteral("state"), text("state"));

	int	endsIn = _wanted.value(QStringLiteral("endsIn"), -1).toInt();

	if (endsIn > 0)
	{
		qint64	end = QDateTime::currentMSecsSinceEpoch() + qint64(endsIn) * 1000;

		if (qAbs(end - _end) > EndSlackMs)
			_end = end;

		activity.insert(QStringLiteral("timestamps"), QJsonObject{ { QStringLiteral("end"), _end } });
	}
	else
		_end = 0;

	QJsonObject	assets;

	for (const char *key : { "largeImage", "largeText", "smallImage", "smallText" })
	{
		QString	value = _wanted.value(QLatin1String(key)).toString();

		if (value.isEmpty())
			continue;

		// large_image, large_text, small_image, small_text.
		QString	name = QString::fromLatin1(key);
		name.replace(QStringLiteral("Image"), QStringLiteral("_image"))
			.replace(QStringLiteral("Text"), QStringLiteral("_text"));

		// The texts are shown on hover and held to 128 like the others; the images are
		// keys or links.
		assets.insert(name.toLower(), fitText(value, name.endsWith(QLatin1String("_text")) ? 128 : 256));
	}

	if (!assets.isEmpty())
		activity.insert(QStringLiteral("assets"), assets);

	// What someone who opens the profile can click (Discord allows two, and does not show
	// them to the user themself): the installer, and the open-source project.
	activity.insert(QStringLiteral("buttons"), QJsonArray{
		QJsonObject{
			{ QStringLiteral("label"), QStringLiteral("Download Pomodoro") },
			{ QStringLiteral("url"), QStringLiteral("https://github.com/elhiba/pomodoro/releases/latest") }
		},
		QJsonObject{
			{ QStringLiteral("label"), QStringLiteral("View on GitHub") },
			{ QStringLiteral("url"), QStringLiteral("https://github.com/elhiba/pomodoro") }
		}
	});

	return activity;
}

void	DiscordPresence::sendActivity()
{
	if (!_ready)
		return;

	QJsonObject	activity = activityJson();
	QByteArray	shown = QJsonDocument(activity).toJson(QJsonDocument::Compact);

	if (shown == _lastSent)
		return;

	_lastSent = shown;
	_lastSentAt = QDateTime::currentMSecsSinceEpoch();

	write(Frame, QJsonObject{
		{ QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY") },
		{ QStringLiteral("args"), QJsonObject{
			{ QStringLiteral("pid"), qint64(QCoreApplication::applicationPid()) },
			{ QStringLiteral("activity"), activity }
		} },
		{ QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces) }
	});
}

void	DiscordPresence::clearActivity()
{
	if (!_ready)
		return;

	write(Frame, QJsonObject{
		{ QStringLiteral("cmd"), QStringLiteral("SET_ACTIVITY") },
		{ QStringLiteral("args"), QJsonObject{
			{ QStringLiteral("pid"), qint64(QCoreApplication::applicationPid()) }
		} },
		{ QStringLiteral("nonce"), QUuid::createUuid().toString(QUuid::WithoutBraces) }
	});

	_lastSent.clear();
}

QString	DiscordPresence::clientId()
{
	return QString::fromUtf8(POMODORO_DISCORD_CLIENT_ID).trimmed();
}

// Every place a running Discord may be listening, most likely first.
//
// Windows: the named pipes \\.\pipe\discord-ipc-N, which QLocalSocket reaches by their
// bare names; sandboxes do not move them.
//
// Elsewhere a socket file. A plain install puts it straight in the runtime directory, but
// a Flatpak or Snap Discord -- the usual kind where people cannot install packages, as on
// school machines -- puts it in its sandbox's own folder underneath. Those are checked by
// name, then the runtime folders are searched a few levels deep for any discord-ipc-N at
// all. Only files that exist are returned; none means Discord is not running, and the
// caller tries again later.
QStringList	DiscordPresence::findTargets()
{
	QStringList	targets;

#ifdef Q_OS_WIN
	for (int index = 0; index < PipeCount; index++)
		targets.append(QStringLiteral("discord-ipc-%1").arg(index));
#else
	QStringList	bases;

	for (const char *variable : { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" })
	{
		QString	folder = qEnvironmentVariable(variable);

		if (!folder.isEmpty())
			bases.append(folder);
	}

	bases.append(QStringLiteral("/run/user/%1").arg(getuid()));
	bases.append(QStringLiteral("/tmp"));
	bases.removeDuplicates();

	auto	add = [&targets](const QString &path)
	{
		QFileInfo	info(path);

		if (info.exists() && !info.isDir() && !targets.contains(info.absoluteFilePath()))
			targets.append(info.absoluteFilePath());
	};

	for (const QString &base : std::as_const(bases))
	{
		for (const char *folder : SandboxFolders)
		{
			for (int index = 0; index < PipeCount; index++)
				add(QDir(base).filePath(QLatin1String(folder) + QStringLiteral("/discord-ipc-%1").arg(index)));
		}
	}

	// Anything else: a new sandbox layout, a renamed client. Bounded, so a crowded /tmp
	// costs nothing noticeable.
	for (const QString &base : std::as_const(bases))
	{
		QDirIterator	it(base, { QStringLiteral("discord-ipc-*") }, QDir::System | QDir::Files | QDir::Hidden,
			QDirIterator::Subdirectories);
		int				looked = 0;

		while (it.hasNext() && looked++ < 4000)
		{
			QString	path = it.next();

			if (QDir(base).relativeFilePath(path).count(QLatin1Char('/')) <= 4)
				add(path);
		}
	}
#endif

	return targets;
}
