#ifndef DISCORD_PRESENCE_HPP
#define DISCORD_PRESENCE_HPP

#include <QByteArray>
#include <QJsonObject>
#include <QLocalSocket>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantMap>

#include <QtQml/qqmlregistration.h>

// Shows what Pomodoro is doing on the user's Discord profile ("Playing Pomodoro --
// Focusing, 12:34 left"), through Discord's local Rich Presence connection.
//
// Nothing to sign in to: the Discord app on the same computer listens on a local pipe
// (discord-ipc-0..9: a named pipe on Windows, a socket in the runtime directory
// elsewhere), and any app may talk to it with Discord's framing -- an opcode and a
// length, both little-endian 32-bit, then JSON. The app is identified by the Discord
// Application ID the build carries, which is also what names it "Pomodoro" on the
// profile. When Discord is not running it simply retries every so often.
//
// Discord accepts at most five activity updates every twenty seconds, so updates are
// coalesced and sent at most every few seconds, and only when something visible changed.
class DiscordPresence : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY stateChanged)
	Q_PROPERTY(bool available READ available CONSTANT)
	Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
	Q_PROPERTY(QString userName READ userName NOTIFY stateChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

	public:
		explicit DiscordPresence(QObject *parent = nullptr);

		bool	enabled() const;
		void	setEnabled(bool enabled);

		// Whether this build carries a Discord Application ID at all.
		bool	available() const;
		bool	connected() const;
		QString	userName() const;
		QString	statusText() const;

		// The activity to show: details, state, largeImage, largeText, smallImage,
		// smallText, and endsIn (seconds from now, for Discord's countdown; absent or
		// negative for none). Sent when it differs from what Discord already shows.
		Q_INVOKABLE void	show(const QVariantMap &activity);

	signals:
		void	stateChanged();

	private:
		enum Opcode
		{
			Handshake = 0,
			Frame = 1,
			Close = 2,
			Ping = 3,
			Pong = 4
		};

		static constexpr int	RetryMs = 15000;
		static constexpr int	MinimumGapMs = 4000;

		// An end time that moved by less than this is the same countdown, not a new one:
		// the timer republishes every second and would otherwise look like a change.
		static constexpr qint64	EndSlackMs = 3000;

		QLocalSocket	_socket;
		QTimer			_retryTimer;
		QTimer			_sendTimer;
		QByteArray		_buffer;

		bool	_enabled = false;
		bool	_ready = false;
		int		_pipe = 0;
		QString	_userName;
		QString	_error;

		QVariantMap	_wanted;
		qint64		_end = 0;
		QByteArray	_lastSent;
		qint64		_lastSentAt = 0;

		void	connectToDiscord();
		void	onConnected();
		void	onReadyRead();
		void	onDisconnected();
		void	handle(int opcode, const QJsonObject &message);
		void	write(int opcode, const QJsonObject &message);
		void	sendActivity();
		void	clearActivity();

		QJsonObject	activityJson();

		static QString	clientId();
		static QString	pipeName(int index);
};

#endif
