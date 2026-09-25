#ifndef SPOTIFY_ENGINE_HPP
#define SPOTIFY_ENGINE_HPP

#include <QJsonObject>
#include <QList>
#include <QNetworkAccessManager>
#include <QObject>
#include <QProcess>
#include <QString>
#include <QTimer>

#include <functional>

// The Spotify player that plays inside Pomodoro: go-librespot, an open-source Spotify
// Connect client, shipped next to the executable and run hidden as a child process.
// It signs in with the user's Spotify account (Premium), shows up in Spotify as a device
// called "Pomodoro", decodes the audio itself and plays it on the default output -- so
// the music comes out of this app, not out of a Spotify app that has to be open.
//
// go-librespot is not an official Spotify client: it signs in as Spotify's own desktop
// app, which is what lets any Premium account use it without the Web API's five-user
// development limit, and which Spotify's terms do not allow. elhiba chose to ship it
// anyway (2026-09-25), knowing Spotify can break it or object to it.
//
// Everything goes through its local REST API on 127.0.0.1: sign-in state from /status,
// playback from /player/*, and an access token from /token, which SpotifyClient uses
// for search and the library. Signing in once is remembered by go-librespot itself
// (state.json in its folder), so later launches are signed in without a browser.
class SpotifyEngine : public QObject
{
	Q_OBJECT

	public:
		enum State
		{
			Missing,	// No go-librespot next to the executable: this build cannot play here.
			Stopped,
			Starting,
			SigningIn,	// Waiting for the user to approve in the browser.
			Ready,
			Failed
		};
		Q_ENUM(State)

		using Reply = std::function<void(int status, const QJsonObject &body)>;

		explicit SpotifyEngine(QObject *parent = nullptr);
		~SpotifyEngine() override;

		State	state() const;
		bool	available() const;
		bool	ready() const;

		// Signed in on an earlier run: worth starting without asking, the sign-in is kept.
		bool	remembered() const;

		QString	username() const;
		QString	deviceId() const;
		QString	errorText() const;

		// Spotify would not play for this account because it is not Premium: it turned
		// the sign-in down as such, or refused every song asked for without playing one.
		// Spotify only lets Premium accounts play in apps other than its own, ads or not,
		// so there is nothing the app can do about it but say so.
		bool	premiumRequired() const;

		static const QString	PremiumRequiredText;

		// Starts the player if needed and, once it asks for a sign-in, opens Spotify's
		// page in the browser.
		void	signIn();

		// Stops the player and forgets the account.
		void	signOut();

		// Starts the player in the background when it was signed in before, so it is
		// ready by the time something is played.
		void	start();

		// Runs `then` once the player is signed in, or straight away with ready() false
		// when it cannot get there.
		void	whenReady(std::function<void()> then);

		// A REST call to the player; body may be empty. The reply's status is 0 when the
		// player could not be reached.
		void	call(const QByteArray &verb, const QString &path, const QJsonObject &body, Reply reply);

		static QString	executablePath();

	signals:
		void	stateChanged();

		// Emitted once, when premiumRequired() turns true.
		void	premiumRequiredFound();

	private:
		static constexpr int	StatusPollMs = 1000;

		// How long the player may take to start and sign in with remembered credentials.
		static constexpr int	StartTimeoutMs = 30000;

		// How long a sign-in in the browser may take.
		static constexpr int	SignInTimeoutMs = 300000;

		// Songs refused in a row, with none played, before the account is taken for a free
		// one. A Premium account can meet the odd song Spotify will not license; not three
		// running with nothing playing in between.
		static constexpr int	RefusalsForFreeAccount = 3;

		QProcess				*_process = nullptr;
		QNetworkAccessManager	_network;
		QTimer					_statusTimer;
		QTimer					_timeout;

		State	_state = Stopped;
		QString	_username;
		QString	_deviceId;
		QString	_errorText;
		QString	_signInUrl;
		bool	_openSignIn = false;
		bool	_premiumRequired = false;
		int		_refusedInARow = 0;

		// Whether go-librespot holds a stored login. Read from its own state.json rather
		// than kept as a flag of ours: two records of one fact drifted apart once (the
		// player signed in, the flag gone), and the panel and the player disagreed.
		bool	_remembered = false;
		quint16	_port = 0;

		QList<std::function<void()>>	_waiting;

		QString	configDir() const;
		bool	readRemembered() const;
		void	launch();
		void	writeConfig();
		void	onOutput();
		void	markPremiumRequired();
		void	onFinished(int exitCode, QProcess::ExitStatus exitStatus);
		void	pollStatus();
		void	setState(State state, const QString &error = QString());
		void	release();
		void	stopProcess();

		static quint16	freePort();
};

#endif
