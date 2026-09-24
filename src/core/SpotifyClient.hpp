#ifndef SPOTIFY_CLIENT_HPP
#define SPOTIFY_CLIENT_HPP

#include <QByteArray>
#include <QDateTime>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTimer>

#include <functional>

#include <QtQml/qqmlregistration.h>

class QNetworkReply;

// Plays the user's own Spotify through the Spotify Web API.
//
// Spotify does not let a third-party desktop app stream its audio: the only thing its
// Web API offers is remote control of a Spotify player the user already has open -- the
// desktop app, the phone, a speaker. So this starts, pauses and reads back what that
// player is doing, and the sound comes out of Spotify itself. Controlling playback needs
// a Premium account; that is Spotify's rule, reported as such.
//
// Signing in is OAuth 2 with PKCE, which needs no client secret: the browser opens
// Spotify's consent page, Spotify redirects to a one-shot listener on
// http://127.0.0.1:47823/callback, and the code is exchanged for tokens. Only the refresh
// token is kept (in the settings file); access tokens live for an hour in memory.
//
// It needs a Client ID from an app registered at developer.spotify.com with exactly that
// redirect URI. A build can carry one (-DPOMODORO_SPOTIFY_CLIENT_ID=...), and the user can
// enter their own in the settings.
class SpotifyClient : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Reached through MusicPlayer.spotify")

	Q_PROPERTY(QString clientId READ clientId WRITE setClientId NOTIFY stateChanged)
	Q_PROPERTY(QString builtInClientId READ builtInClientId CONSTANT)
	Q_PROPERTY(QString redirectUri READ redirectUri CONSTANT)
	Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
	Q_PROPERTY(bool connecting READ connecting NOTIFY stateChanged)
	Q_PROPERTY(QString accountName READ accountName NOTIFY stateChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

	public:
		explicit SpotifyClient(QObject *parent = nullptr);

		QString	clientId() const;
		QString	builtInClientId() const;
		QString	redirectUri() const;
		bool	connected() const;
		bool	connecting() const;
		QString	accountName() const;
		QString	statusText() const;

		QString	track() const;
		QString	artist() const;
		bool	isPlaying() const;

		void	setClientId(const QString &clientId);

		// Starts the given spotify:playlist/album/artist/track URI, or resumes whatever the
		// player last had when it is empty. Answers with playbackStarted/playbackFailed.
		void	play(const QString &uri);
		void	pause();

		// Reads back what is playing every few seconds, for the now-playing line.
		void	setPolling(bool polling);

		// Accepts a spotify: URI or an open.spotify.com link and returns the URI form, or
		// an empty string for anything else.
		Q_INVOKABLE static QString	toUri(const QString &text);

	public slots:
		void	connectAccount();
		void	disconnectAccount();

	signals:
		void	stateChanged();
		void	nowPlayingChanged();
		void	playbackStarted();
		void	playbackFailed(const QString &reason);

	private:
		using Handler = std::function<void(int status, const QJsonObject &body, const QString &error)>;

		static constexpr quint16	CallbackPort = 47823;
		static constexpr int		AuthTimeoutMs = 180000;
		static constexpr int		PollIntervalMs = 5000;

		QNetworkAccessManager	_network;
		QTcpServer				_callbackServer;
		QTimer					_authTimeout;
		QTimer					_pollTimer;

		QString		_clientId;
		QString		_refreshToken;
		QString		_accessToken;
		QDateTime	_accessExpiry;
		QString		_accountName;
		QString		_errorText;

		QByteArray	_verifier;
		QByteArray	_state;
		bool		_connecting = false;
		bool		_refreshing = false;
		QList<std::function<void()>>	_waitingForToken;

		QString	_track;
		QString	_artist;
		bool	_isPlaying = false;

		void	onCallbackConnection();
		void	exchangeCode(const QByteArray &code);
		void	refreshAccessToken(std::function<void()> then);
		void	storeTokens(const QJsonObject &body);
		void	fetchAccount();
		void	poll();
		void	playOn(const QString &uri, const QString &deviceId);
		void	finishConnecting(const QString &error);

		// An authorised Web API call. Refreshes the access token first when it has run out,
		// and once more if Spotify answers 401 anyway.
		void	api(const QByteArray &verb, const QString &path, const QJsonObject &body,
					Handler handler, bool retried = false);

		static QString	reasonFor(int status, const QJsonObject &body);
};

#endif
