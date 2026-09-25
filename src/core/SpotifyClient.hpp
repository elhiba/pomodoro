#ifndef SPOTIFY_CLIENT_HPP
#define SPOTIFY_CLIENT_HPP

#include <QByteArray>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <QTimer>
#include <QVariantList>

#include <functional>

#include <QtQml/qqmlregistration.h>

#include "SpotifyEngine.hpp"

class QNetworkReply;

// Spotify for the music panel, in one of two ways.
//
// When the build ships the built-in player (SpotifyEngine, go-librespot), Spotify plays
// inside Pomodoro: signing in, play, pause, skip and what is playing all go to that
// player. No Client ID, no developer set-up, and the sound comes out of this app.
//
// Spotify refuses every Web API call made with that player's token (429 on search, the
// library, even /v1/me; its internal search endpoints answer 403 -- checked 2026-09-25),
// so with the player alone there is no search. Music is picked from a shelf of playlists
// and from Spotify links the user pastes, named through the public oEmbed endpoint. A
// user who adds a developer Client ID of their own gets search and their library back:
// the Web API sign-in below is then used for lookups only, and playback stays here.
//
// Without it, everything below applies: the Web API remote-controls a Spotify player
// the user already has open.
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

	// True when Spotify plays inside Pomodoro through the built-in player.
	Q_PROPERTY(bool builtInPlayer READ builtInPlayer CONSTANT)

	// Whether search and the library can be used: always without the built-in player,
	// and with it only once a developer Client ID has been signed in for lookups.
	Q_PROPERTY(bool canSearch READ canSearch NOTIFY stateChanged)

	// Spotify's "Liked Songs" as something the built-in player can play.
	Q_PROPERTY(QString likedSongsUri READ likedSongsUri NOTIFY stateChanged)
	Q_PROPERTY(bool connected READ connected NOTIFY stateChanged)
	Q_PROPERTY(bool connecting READ connecting NOTIFY stateChanged)
	Q_PROPERTY(QString accountName READ accountName NOTIFY stateChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY stateChanged)

	// Signed in before the library scopes were asked for: search works, the user's own
	// playlists and history need one more trip through Connect.
	Q_PROPERTY(bool needsReconnect READ needsReconnect NOTIFY stateChanged)

	// What the music panel lists: search results or a library section, as maps with kind,
	// uri, title, subtitle and image. Replaced as a whole on every search or section.
	Q_PROPERTY(QVariantList results READ results NOTIFY resultsChanged)
	Q_PROPERTY(bool searching READ searching NOTIFY resultsChanged)
	Q_PROPERTY(QString resultsError READ resultsError NOTIFY resultsChanged)

	// A playlist, album or Liked Songs opened to show its songs: results then holds the
	// songs, and openedUri/openedTitle say whose they are. Empty when nothing is open.
	Q_PROPERTY(QString openedUri READ openedUri NOTIFY resultsChanged)
	Q_PROPERTY(QString openedTitle READ openedTitle NOTIFY resultsChanged)

	// The cover of what is playing now, for the panel.
	Q_PROPERTY(QString artUrl READ artUrl NOTIFY nowPlayingChanged)

	// Where the song is playing from: a playlist or album name, "Liked Songs", or
	// "Search". Empty until known.
	Q_PROPERTY(QString contextName READ contextName NOTIFY nowPlayingChanged)

	// Whether the song playing now is in the user's Liked Songs. Saving needs the Web
	// API, so it only works where search does (canSearch); the heart is hidden otherwise.
	Q_PROPERTY(bool currentLiked READ currentLiked NOTIFY nowPlayingChanged)
	Q_PROPERTY(bool hasCurrentTrack READ hasCurrentTrack NOTIFY nowPlayingChanged)

	public:
		explicit SpotifyClient(QObject *parent = nullptr);

		QString	clientId() const;
		QString	builtInClientId() const;
		QString	redirectUri() const;
		bool	builtInPlayer() const;
		bool	canSearch() const;
		QString	likedSongsUri() const;
		bool	connected() const;
		bool	connecting() const;
		QString	accountName() const;
		QString	statusText() const;

		QString	track() const;
		QString	artist() const;
		bool	isPlaying() const;
		QString	artUrl() const;

		// Where in the current song playback is, and its length, in milliseconds. The
		// position is the last one read plus the time since, while playing.
		qint64	positionMs() const;
		qint64	durationMs() const;
		void	seek(qint64 milliseconds);

		QString	contextName() const;
		bool	currentLiked() const;
		bool	hasCurrentTrack() const;

		QString	openedUri() const;
		QString	openedTitle() const;

		bool			needsReconnect() const;
		QVariantList	results() const;
		bool			searching() const;
		QString			resultsError() const;

		void	setClientId(const QString &clientId);

		// Starts the given spotify:playlist/album/artist/track URI, or resumes whatever the
		// player last had when it is empty. Answers with playbackStarted/playbackFailed.
		void	play(const QString &uri);
		void	pause();
		void	next();
		void	previous();

		// Starts results[index]. A song plays with the other songs of the same list queued
		// after it, so skipping moves through the list; anything else is played as a
		// whole. Answers with playbackStarted/playbackFailed like play().
		void	playResult(int index);

		// Reads back what is playing every few seconds, for the now-playing line.
		void	setPolling(bool polling);

		// The built-in player's volume, 0 to 1. Does nothing for a remote-controlled
		// Spotify, whose volume is its own.
		void	setPlayerVolume(qreal volume);

		// Accepts a spotify: URI or an open.spotify.com link and returns the URI form, or
		// an empty string for anything else.
		Q_INVOKABLE static QString	toUri(const QString &text);

		// Names a pasted Spotify link through the public oEmbed endpoint, which needs no
		// sign-in. Answers with linkLookedUp.
		Q_INVOKABLE void	lookUpLink(const QString &text);

		// Lists the songs of a playlist, album or Liked Songs in results (built-in player
		// only; it reads them without the Web API). closeContext() puts back what was
		// listed before; playOpened() plays the whole list from the top.
		Q_INVOKABLE void	openContext(const QString &uri, const QString &title);

		// Puts a song after the one playing. Answers with notice().
		Q_INVOKABLE void	addToQueue(const QString &uri);

		// Adds the song playing now to Liked Songs, or takes it out again.
		Q_INVOKABLE void	toggleLike();
		Q_INVOKABLE void	closeContext();
		Q_INVOKABLE void	playOpened();

	public slots:
		void	connectAccount();
		void	disconnectAccount();

		// The developer Client ID sign-in, which with the built-in player is only used for
		// search and the library.
		void	connectSearch();
		void	disconnectSearch();

		void	search(const QString &query);

		// "playlists", "liked", "recent" or "top".
		void	loadLibrary(const QString &section);

	signals:
		void	stateChanged();
		void	nowPlayingChanged();
		void	playbackStarted();
		void	playbackFailed(const QString &reason);
		void	resultsChanged();

		// A short confirmation or refusal for the panel to show and let fade ("Added to
		// the queue").
		void	notice(const QString &text);

		// uri is empty when the link was not one; error then says why.
		void	linkLookedUp(const QString &uri, const QString &title, const QString &image, const QString &error);

	private:
		using Handler = std::function<void(int status, const QJsonObject &body, const QString &error)>;

		static constexpr quint16	CallbackPort = 47823;
		static constexpr int		AuthTimeoutMs = 180000;
		static constexpr int		PollIntervalMs = 5000;

		// The built-in player answers locally, so it is asked more often.
		static constexpr int		PlayerPollIntervalMs = 2000;

		// Tokens from the built-in player last an hour; fetched again well before that.
		static constexpr int		PlayerTokenSeconds = 45 * 60;

		// Songs queued after the one tapped in a list.
		static constexpr int		MaximumQueued = 30;

		// The player lists a playlist in the background; asked again this often, this
		// many times, while it fills in names and covers.
		static constexpr int		ContextPollMs = 600;
		static constexpr int		ContextPollAttempts = 25;

		QNetworkAccessManager	_network;
		SpotifyEngine			_engine;
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
		QString	_trackUri;
		bool	_liked = false;
		QString	_artist;
		QString	_artUrl;
		bool	_isPlaying = false;

		qint64			_positionMs = 0;
		qint64			_durationMs = 0;
		QElapsedTimer	_positionClock;

		// The list playing now, and the names of lists started from the panel, so the
		// player's bare URI can be shown as the name the user picked it by.
		QString					_contextUri;
		QHash<QString, QString>	_contextNames;

		QString			_openedUri;
		QString			_openedTitle;
		QVariantList	_resultsBeforeOpening;
		QString			_errorBeforeOpening;

		QString			_grantedScopes;
		QVariantList	_results;
		bool			_searching = false;
		QString			_resultsError;

		// Bumped by every search or section load, so a slow answer to an older request
		// cannot overwrite a newer one.
		int				_resultsGeneration = 0;

		void	onCallbackConnection();
		void	exchangeCode(const QByteArray &code);
		void	refreshAccessToken(std::function<void()> then);
		void	storeTokens(const QJsonObject &body);
		void	fetchAccount();
		void	poll();
		void	playOn(const QJsonObject &body, const QString &deviceId);

		// The built-in player's side of play/pause/skip/poll.
		void	playerCommand(const QString &path, const QJsonObject &body, bool startsPlayback);
		void	playerQueue(QStringList uris);
		void	playerPoll();
		void	fetchContext(const QString &uri, int generation, int attempt);
		void	setContext(const QString &uri, const QString &reportedName);
		void	setTrackUri(const QString &uri);
		void	fetchResults(const QString &path, std::function<QVariantList(const QJsonObject &)> parse);
		void	finishConnecting(const QString &error);
		void	startWebSignIn();

		// An authorised Web API call. Refreshes the access token first when it has run out,
		// and once more if Spotify answers 401 anyway.
		void	api(const QByteArray &verb, const QString &path, const QJsonObject &body,
					Handler handler, bool retried = false);

		static QString	reasonFor(int status, const QJsonObject &body);
};

#endif
