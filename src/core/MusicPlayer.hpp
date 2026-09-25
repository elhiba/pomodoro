#ifndef MUSIC_PLAYER_HPP
#define MUSIC_PLAYER_HPP

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QNetworkInformation>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <QtQml/qqmlregistration.h>

#include "MediaControls.hpp"
#include "SpotifyClient.hpp"
#include "StreamMetadata.hpp"
#include "YtDlp.hpp"

// The background stream. QMediaPlayer here rather than QSoundEffect: this is a
// compressed network stream of unbounded length, the opposite of the short PCM cues
// SoundPlayer handles.
//
// The whole point of the status property is that a stream which will not start has to
// say so. A dead URL must not leave the UI sitting on a play icon forever, so there is
// a watchdog as well as the usual error signal.
//
// Once the user has asked for music, though, the answer to the network going away is
// to keep trying, not to give up: a dropped connection schedules a reconnect with a
// growing delay, and the network coming back cuts that delay short. Only pause() and
// stop() end the attempts. A URL that is wrong rather than unreachable still fails
// outright, since no amount of retrying fixes a typo.
//
// The FFmpeg backend cannot be trusted to notice a stream that has quietly stopped
// arriving: given a stalled or reset connection it will happily report itself still
// playing, its clock ticking along, for as long as you leave it. So the liveness check
// does not come from the player at all. StreamMetadata already opens a second, short
// connection to the same URL every so often; whether that probe succeeds is the real
// answer to "is the stream still there", and a run of failed probes is what triggers a
// reconnect. To avoid punishing a server that refuses a second listener, probe failures
// only count once at least one probe has succeeded, proving the server allows it.
//
// The source decides where the sound comes from, and the rest of the app never needs to
// know which it is:
//   * a stream URL (the radio list or a custom link) plays here as described above;
//   * a YouTube link is first turned into a direct audio URL by yt-dlp, again on every
//     (re)connect because those URLs expire, and then plays here the same way;
//   * "spotify:" or "spotify:<uri>" is handed to SpotifyClient, which drives the user's
//     own Spotify player -- the sound comes out of Spotify, not this process.
class MusicPlayer : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(QString source READ source WRITE setSource NOTIFY sourceChanged)
	Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)
	Q_PROPERTY(Status status READ status NOTIFY statusChanged)
	Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
	Q_PROPERTY(bool active READ active NOTIFY statusChanged)
	Q_PROPERTY(bool failed READ failed NOTIFY statusChanged)
	Q_PROPERTY(bool reconnecting READ reconnecting NOTIFY statusChanged)
	Q_PROPERTY(int retryAttempt READ retryAttempt NOTIFY statusChanged)

	// The helpers behind the YouTube and Spotify sources, for the settings panel.
	Q_PROPERTY(YtDlp *youtube READ youtube CONSTANT)
	Q_PROPERTY(SpotifyClient *spotify READ spotify CONSTANT)
	Q_PROPERTY(SourceKind sourceKind READ sourceKind NOTIFY sourceChanged)

	// Cover art or thumbnail of what is playing, empty for a radio stream.
	Q_PROPERTY(QString artUrl READ artUrl NOTIFY nowPlayingChanged)

	// Whether next/previous mean anything for this source by itself. The radio list skips
	// between stations in QML, so it is not counted here.
	Q_PROPERTY(bool canSkip READ canSkip NOTIFY queueChanged)

	// Where in the song or video playback is, and how long it is, in milliseconds, for
	// the timeline. seekable is false for radio and live streams, which have no length.
	Q_PROPERTY(qint64 position READ position NOTIFY positionChanged)
	Q_PROPERTY(qint64 duration READ duration NOTIFY positionChanged)
	Q_PROPERTY(bool seekable READ seekable NOTIFY positionChanged)

	// What the stream says is on. Empty until it has said anything.
	Q_PROPERTY(QString stationName READ stationName NOTIFY nowPlayingChanged)
	Q_PROPERTY(QString genre READ genre NOTIFY nowPlayingChanged)
	Q_PROPERTY(QString title READ title NOTIFY nowPlayingChanged)

	public:
		enum Status
		{
			Idle,
			Connecting,
			Playing,
			Paused,
			Reconnecting,
			Failed
		};
		Q_ENUM(Status)

		enum SourceKind
		{
			Stream,
			YouTube,
			Spotify
		};
		Q_ENUM(SourceKind)

		explicit MusicPlayer(QObject *parent = nullptr);

		QString	source() const;
		qreal	volume() const;
		Status	status() const;
		QString	statusText() const;

		// True while the user has asked for music, whether or not a note has arrived yet.
		bool	active() const;
		bool	failed() const;
		bool	reconnecting() const;
		int		retryAttempt() const;

		QString	stationName() const;
		QString	genre() const;
		QString	title() const;

		YtDlp			*youtube() const;
		SpotifyClient	*spotify() const;
		SourceKind		sourceKind() const;
		QString			artUrl() const;
		bool			canSkip() const;

		// Plays items[index] from a YouTube listing with the rest of the videos queued
		// around it, so next and previous walk the list. Playlists in the listing are
		// skipped over.
		Q_INVOKABLE void	playYouTubeQueue(const QVariantList &items, int index);

		// Starts one of the Spotify panel's results.
		Q_INVOKABLE void	playSpotifyResult(int index);

		// Drops the music to a murmur for the given time, so the alarm is heard over it.
		Q_INVOKABLE void	duck(int milliseconds);

		qint64	position() const;
		qint64	duration() const;
		bool	seekable() const;

		// Jumps to a point in the song or video. Does nothing when not seekable.
		Q_INVOKABLE void	seek(qint64 milliseconds);

		void	setSource(const QString &source);
		void	setVolume(qreal volume);

	public slots:
		void	play();
		void	pause();
		void	toggle();
		void	stop();
		void	next();
		void	previous();

	signals:
		void	sourceChanged();
		void	volumeChanged();
		void	statusChanged();
		void	nowPlayingChanged();
		void	queueChanged();
		void	positionChanged();

		// The YouTube queue moved to another video. Main.qml records it as the YouTube
		// choice so the app comes back to it next time.
		void	youtubeTrackChanged(const QString &url);

	private slots:
		void	onPlaybackStateChanged();
		void	onMediaStatusChanged();
		void	onErrorOccurred(QMediaPlayer::Error error, const QString &message);
		void	onBackendMetaDataChanged();
		void	onWatchdogTimeout();
		void	onRetryTimeout();
		void	onProbeSucceeded();
		void	onProbeFailed();
		void	onReachabilityChanged(QNetworkInformation::Reachability reachability);
		void	onMetadataChanged();
		void	onYouTubeResolved(const QUrl &stream, const QString &title, const QString &channel, bool live,
					const QString &thumbnail);
		void	onYouTubeFailed(const QString &reason);
		void	onSpotifyStarted();
		void	onSpotifyFailed(const QString &reason);

	private:
		// Long enough for a slow connection to a live stream, short enough that a dead
		// host does not leave the UI pretending it is about to work.
		static constexpr int	WatchdogMs = 20000;

		// How long yt-dlp gets to turn a YouTube link into a stream. Its first run on a
		// machine unpacks itself, which can take several seconds on its own.
		static constexpr int	YouTubeResolveMs = 60000;

		// The reconnect delay doubles from the first to the cap: 2, 4, 8, 16, 30, 30...
		// Quick enough that a blip is barely audible, slow enough not to hammer a server
		// that is having a bad day.
		static constexpr int	FirstRetryMs = 2000;
		static constexpr int	MaximumRetryMs = 30000;

		// When the network comes back, a beat for DNS and routes to settle before the
		// next attempt, rather than firing into a half-raised interface.
		static constexpr int	NetworkBackRetryMs = 750;

		// How many probe failures in a row, while the player still claims to be playing,
		// are taken as proof the stream is gone. Two rather than one rides out a single
		// dropped request without tearing a healthy stream down.
		static constexpr int	ProbeFailuresForDrop = 2;

		// The timeline moves in quarter seconds: smooth enough for a bar a few hundred
		// pixels wide, and a quarter of the redraws of a per-frame update.
		static constexpr int	PositionTickMs = 250;

		// How loud the music stays, as a share of its volume, while it is ducked.
		static constexpr qreal	DuckLevel = 0.15;

		// Recreated on every connection attempt rather than reused. The FFmpeg backend
		// cannot be talked out of a bad connection by handing it a new URL: it clings to
		// the buffered remains of the old stream and reports itself happily playing them
		// while never touching the network again. A fresh player each time is the only
		// reliable way to force a genuine reconnect. Owned through the QObject parent.
		QMediaPlayer	*_player = nullptr;
		QAudioOutput	*_output = nullptr;
		QTimer			_watchdog;
		QTimer			_retryTimer;
		QTimer			_duckTimer;

		// Ticks the timeline along while something plays; stopped otherwise, so an idle
		// app redraws nothing.
		QTimer			_positionTimer;

		StreamMetadata	_metadata;
		MediaControls	*_controls = nullptr;
		YtDlp			*_ytDlp = nullptr;
		SpotifyClient	*_spotify = nullptr;

		SourceKind	_kind = Stream;

		// A YouTube source's live flag, from the last resolve. A video that is not live
		// reaching its end simply starts again; a live stream ending is a dropped
		// connection like any other.
		bool	_youtubeLive = false;

		// The Spotify context last started, so that pressing play after a pause resumes
		// where it was instead of starting the playlist over.
		QString	_spotifyStarted;

		// The YouTube queue: video URLs and their thumbnails, and where in it we are.
		QStringList	_queue;
		QStringList	_queueImages;
		int			_queueIndex = -1;
		QString		_youtubeThumbnail;

		QString	_source;
		qreal	_volume = 0.5;
		bool	_ducked = false;
		Status	_status = Idle;
		QString	_errorText;

		// What the user asked for, as opposed to what the network is currently doing.
		bool	_wantsPlayback = false;

		// True between a failure and the retry it scheduled. While it is set the player
		// is deliberately stopped, and its state changes must not be read as news.
		bool	_retryPending = false;
		int		_retryAttempt = 0;

		// The liveness probe's tally. Failures only count once one probe has succeeded,
		// which is what _probeConfirmed records: proof the server tolerates the second
		// connection at all, so a later failure means the stream and not the policy.
		bool	_probeConfirmed = false;
		int		_probeFailures = 0;

		// Title and station as far as the backend itself will say, kept separately from
		// the ICY reader so the two sources can be combined in one place.
		QString	_backendTitle;
		QString	_backendStation;

		void	setStatus(Status status, const QString &errorText = QString());
		void	refreshStatus();
		void	rebuildPlayer();
		void	openStream();
		void	startPlayer(const QUrl &url);
		void	moveQueue(int step);
		void	scheduleRetry(const QString &reason);
		void	cancelRetry();
		void	updateNowPlaying();
		void	updateControls();

		bool	networkLooksDown() const;
		int		retryDelayMs() const;
};

#endif
