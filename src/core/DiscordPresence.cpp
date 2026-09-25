#include "DiscordPresence.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSettings>
#include <QUuid>
#include <QtEndian>

#ifndef POMODORO_DISCORD_CLIENT_ID
#define POMODORO_DISCORD_CLIENT_ID ""
#endif

namespace
{
	const char *const	KeyEnabled = "discord/enabled";

	// Discord looks at ten pipes in turn; a second Discord (PTB, Canary) takes the next.
	constexpr int	PipeCount = 10;
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
		_pipe = 0;
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
		return _userName.isEmpty()
			? QStringLiteral("Connected to Discord")
			: QStringLiteral("Connected as %1").arg(_userName);

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

	_buffer.clear();
	_socket.connectToServer(pipeName(_pipe));
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

	// Next pipe straight away; once all ten are tried, wait and start over.
	if (!wasReady && ++_pipe < PipeCount)
	{
		QTimer::singleShot(0, this, &DiscordPresence::connectToDiscord);
		return;
	}

	_pipe = 0;
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
		QString	value = _wanted.value(QLatin1String(key)).toString().trimmed().left(128);

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

		assets.insert(name.toLower(), value.left(256));
	}

	if (!assets.isEmpty())
		activity.insert(QStringLiteral("assets"), assets);

	activity.insert(QStringLiteral("buttons"), QJsonArray{ QJsonObject{
		{ QStringLiteral("label"), QStringLiteral("Get Pomodoro") },
		{ QStringLiteral("url"), QStringLiteral("https://github.com/elhiba/pomodoro/releases/latest") }
	} });

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

// Windows: the named pipe \\.\pipe\discord-ipc-N, which QLocalSocket reaches by its bare
// name. Elsewhere a socket file in the first of the usual runtime folders that is set.
QString	DiscordPresence::pipeName(int index)
{
	QString	name = QStringLiteral("discord-ipc-%1").arg(index);

#ifdef Q_OS_WIN
	return name;
#else
	for (const char *variable : { "XDG_RUNTIME_DIR", "TMPDIR", "TMP", "TEMP" })
	{
		QString	folder = qEnvironmentVariable(variable);

		if (!folder.isEmpty())
			return QDir(folder).filePath(name);
	}

	return QStringLiteral("/tmp/") + name;
#endif
}
