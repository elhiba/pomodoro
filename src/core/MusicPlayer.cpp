#include "MusicPlayer.hpp"

#include <QDebug>
#include <QMediaMetaData>

MusicPlayer::MusicPlayer(QObject *parent)
	: QObject(parent)
{
	_watchdog.setSingleShot(true);
	_watchdog.setInterval(WatchdogMs);

	_retryTimer.setSingleShot(true);
	_duckTimer.setSingleShot(true);
	_positionTimer.setInterval(PositionTickMs);

	connect(&_positionTimer, &QTimer::timeout, this, &MusicPlayer::positionChanged);

	rebuildPlayer();

	connect(&_duckTimer, &QTimer::timeout, this, [this]()
	{
		_ducked = false;
		_output->setVolume(_volume);

		if (_kind == Spotify)
			_spotify->setPlayerVolume(_volume);
	});

	connect(&_watchdog, &QTimer::timeout, this, &MusicPlayer::onWatchdogTimeout);
	connect(&_retryTimer, &QTimer::timeout, this, &MusicPlayer::onRetryTimeout);
	connect(&_metadata, &StreamMetadata::changed, this, &MusicPlayer::onMetadataChanged);
	connect(&_metadata, &StreamMetadata::probeSucceeded, this, &MusicPlayer::onProbeSucceeded);
	connect(&_metadata, &StreamMetadata::probeFailed, this, &MusicPlayer::onProbeFailed);

	// The reachability backend is optional. Without one the backoff timer carries the
	// reconnects on its own; with one, the network coming back cuts the wait short.
	if (QNetworkInformation::loadDefaultBackend())
	{
		connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged,
			this, &MusicPlayer::onReachabilityChanged);
	}

	// The desktop's media controls. Their requests go through the same slots the
	// buttons use, so the OS can do nothing the UI cannot.
	_controls = MediaControls::create(this);

	connect(_controls, &MediaControls::playRequested, this, &MusicPlayer::play);
	connect(_controls, &MediaControls::pauseRequested, this, &MusicPlayer::pause);
	connect(_controls, &MediaControls::toggleRequested, this, &MusicPlayer::toggle);
	connect(_controls, &MediaControls::stopRequested, this, &MusicPlayer::stop);

	_ytDlp = new YtDlp(this);

	connect(_ytDlp, &YtDlp::resolved, this, &MusicPlayer::onYouTubeResolved);
	connect(_ytDlp, &YtDlp::resolveFailed, this, &MusicPlayer::onYouTubeFailed);

	_spotify = new SpotifyClient(this);

	connect(_spotify, &SpotifyClient::playbackStarted, this, &MusicPlayer::onSpotifyStarted);
	connect(_spotify, &SpotifyClient::playbackFailed, this, &MusicPlayer::onSpotifyFailed);
	connect(_spotify, &SpotifyClient::nowPlayingChanged, this, &MusicPlayer::updateNowPlaying);
	connect(_spotify, &SpotifyClient::nowPlayingChanged, this, &MusicPlayer::positionChanged);

	// Signed out while Spotify was on: nothing can play or resume any more, so the player
	// goes back to idle instead of showing a song it can no longer reach.
	connect(_spotify, &SpotifyClient::stateChanged, this, [this]()
	{
		if (_kind == Spotify && !_spotify->connected() && !_spotify->connecting() && _status != Idle)
			stop();
	});
}

QString	MusicPlayer::source() const
{
	return _source;
}

qreal	MusicPlayer::volume() const
{
	return _volume;
}

MusicPlayer::Status	MusicPlayer::status() const
{
	return _status;
}

QString	MusicPlayer::statusText() const
{
	switch (_status)
	{
		case Connecting:
			return QStringLiteral("Connecting…");
		case Playing:
			return QStringLiteral("Playing");
		case Paused:
			return QStringLiteral("Paused");
		case Reconnecting:
			if (networkLooksDown())
				return QStringLiteral("Waiting for the network…");

			if (_errorText.isEmpty())
				return QStringLiteral("Reconnecting… (attempt %1)").arg(_retryAttempt);

			return QStringLiteral("Reconnecting… (attempt %1) — %2").arg(_retryAttempt).arg(_errorText);
		case Failed:
			return _errorText.isEmpty() ? QStringLiteral("Could not play the stream") : _errorText;
		case Idle:
		default:
			return _source.isEmpty() ? QStringLiteral("No stream set") : QStringLiteral("Stopped");
	}
}

bool	MusicPlayer::active() const
{
	return _status == Connecting || _status == Playing || _status == Reconnecting;
}

bool	MusicPlayer::failed() const
{
	return _status == Failed;
}

bool	MusicPlayer::reconnecting() const
{
	return _status == Reconnecting;
}

int	MusicPlayer::retryAttempt() const
{
	return _retryAttempt;
}

YtDlp	*MusicPlayer::youtube() const
{
	return _ytDlp;
}

SpotifyClient	*MusicPlayer::spotify() const
{
	return _spotify;
}

MusicPlayer::SourceKind	MusicPlayer::sourceKind() const
{
	return _kind;
}

QString	MusicPlayer::artUrl() const
{
	switch (_kind)
	{
		case Spotify:
			return _spotify->artUrl();
		case YouTube:
			return _youtubeThumbnail;
		case Stream:
		default:
			return QString();
	}
}

bool	MusicPlayer::canSkip() const
{
	if (_kind == Spotify)
		return true;

	return _kind == YouTube && _queue.size() > 1;
}

void	MusicPlayer::playYouTubeQueue(const QVariantList &items, int index)
{
	QStringList	urls;
	QStringList	images;
	int			start = 0;

	for (int row = 0; row < items.size(); row++)
	{
		QVariantMap	item = items.at(row).toMap();

		if (item.value(QStringLiteral("kind")).toString() != QLatin1String("video"))
			continue;

		if (row == index)
			start = urls.size();

		urls.append(item.value(QStringLiteral("url")).toString());
		images.append(item.value(QStringLiteral("image")).toString());
	}

	if (urls.isEmpty())
		return;

	_queue = urls;
	_queueImages = images;
	_queueIndex = start;

	emit queueChanged();

	moveQueue(0);
}

void	MusicPlayer::playSpotifyResult(int index)
{
	if (_kind != Spotify)
		return;

	_wantsPlayback = true;
	setStatus(Connecting);

	// From here on, pressing play after a pause resumes this rather than starting the
	// source's own playlist over.
	_spotifyStarted = _source == QLatin1String("spotify:") ? QString() : _source;

	_spotify->playResult(index);
}

void	MusicPlayer::next()
{
	if (_kind == Spotify)
		_spotify->next();
	else if (_kind == YouTube && _queue.size() > 1)
		moveQueue(1);
}

void	MusicPlayer::previous()
{
	if (_kind == Spotify)
		_spotify->previous();
	else if (_kind == YouTube && _queue.size() > 1)
		moveQueue(-1);
}

// Steps through the queue, wrapping at both ends, and plays where it lands.
void	MusicPlayer::moveQueue(int step)
{
	int	count = static_cast<int>(_queue.size());

	_queueIndex = ((_queueIndex + step) % count + count) % count;

	QString	url = _queue.at(_queueIndex);

	_youtubeThumbnail = _queueImages.value(_queueIndex);

	// Main.qml writes this into the settings, which sets it as the source. Doing it here
	// as well covers a caller that does not listen.
	emit youtubeTrackChanged(url);

	if (_source != url)
		setSource(url);

	if (!_wantsPlayback)
		play();
}

QString	MusicPlayer::stationName() const
{
	if (_kind == Spotify)
		return _spotify->artist();

	if (!_metadata.stationName().isEmpty())
		return _metadata.stationName();

	return _backendStation;
}

QString	MusicPlayer::genre() const
{
	switch (_kind)
	{
		case Spotify:
			return QStringLiteral("Spotify");
		case YouTube:
			return _youtubeLive ? QStringLiteral("YouTube live") : QStringLiteral("YouTube");
		case Stream:
		default:
			return _metadata.genre();
	}
}

QString	MusicPlayer::title() const
{
	if (_kind == Spotify)
		return _spotify->track();

	QString	title = _metadata.title().isEmpty() ? _backendTitle : _metadata.title();
	QString	station = stationName();

	// Many stations prefix every title with their own name, "Lofi Music - Playlist 5".
	// The name is already shown on its own line, so it need not be said twice.
	if (!station.isEmpty() && title.size() > station.size() + 3
		&& title.startsWith(station + QStringLiteral(" - "), Qt::CaseInsensitive))
	{
		title = title.mid(station.size() + 3).trimmed();
	}

	return title;
}

void	MusicPlayer::setSource(const QString &source)
{
	QString	trimmed = source.trimmed();

	if (_source == trimmed)
		return;

	// Changing the stream out from under a playing one starts the new one instead.
	// Stopped before the source changes, so it is the old kind of source that is stopped.
	bool	wasPlaying = _wantsPlayback;

	stop();

	_source = trimmed;
	_spotifyStarted.clear();
	_youtubeLive = false;

	// A source picked from outside the queue ends the queue.
	if (_queueIndex < 0 || _queue.value(_queueIndex) != _source)
	{
		bool	hadQueue = !_queue.isEmpty();

		_queue.clear();
		_queueImages.clear();
		_queueIndex = -1;
		_youtubeThumbnail.clear();

		if (hadQueue)
			emit queueChanged();
	}

	if (_source.startsWith(QLatin1String("spotify:")))
		_kind = Spotify;
	else if (YtDlp::handles(QUrl(_source)))
	{
		_kind = YouTube;
		_ytDlp->updateIfStale();
	}
	else
		_kind = Stream;

	// Deliberately NOT handed to QMediaPlayer here. The FFmpeg backend opens the URL
	// as soon as it is set, to probe the format, which would connect to the stream on
	// every launch before the user has asked for any music. openStream() sets it.
	_player->setSource(QUrl());

	emit sourceChanged();
	emit statusChanged();

	if (wasPlaying && !_source.isEmpty())
		play();
}

void	MusicPlayer::setVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_volume, volume))
		return;

	_volume = volume;
	_output->setVolume(_ducked ? _volume * DuckLevel : _volume);

	if (_kind == Spotify)
		_spotify->setPlayerVolume(_ducked ? _volume * DuckLevel : _volume);

	emit volumeChanged();
}

qint64	MusicPlayer::position() const
{
	switch (_kind)
	{
		case YouTube:
			return _player->position();
		case Spotify:
			return _spotify->positionMs();
		default:
			return 0;
	}
}

qint64	MusicPlayer::duration() const
{
	switch (_kind)
	{
		case YouTube:
			return _youtubeLive ? 0 : _player->duration();
		case Spotify:
			return _spotify->durationMs();
		default:
			return 0;
	}
}

bool	MusicPlayer::seekable() const
{
	if (duration() <= 0 || _status == Idle || _status == Failed)
		return false;

	return _kind == Spotify || (_kind == YouTube && _player->isSeekable());
}

void	MusicPlayer::seek(qint64 milliseconds)
{
	if (!seekable())
		return;

	milliseconds = qBound(qint64(0), milliseconds, duration());

	if (_kind == Spotify)
		_spotify->seek(milliseconds);
	else
		_player->setPosition(milliseconds);

	emit positionChanged();
}

void	MusicPlayer::duck(int milliseconds)
{
	// The alarm is a short sample at its own volume, and music at full volume buried it
	// -- worse at the end of a break, when "focus only" music starts again at the same
	// moment the alarm sounds. A remote-controlled Spotify plays in its own app, whose
	// volume is not ours; the built-in player's is.
	if ((_kind == Spotify && !_spotify->builtInPlayer()) || milliseconds <= 0)
		return;

	_ducked = true;
	_output->setVolume(_volume * DuckLevel);

	if (_kind == Spotify)
		_spotify->setPlayerVolume(_volume * DuckLevel);

	_duckTimer.start(milliseconds);
}

void	MusicPlayer::play()
{
	if (_source.isEmpty())
	{
		setStatus(Failed, QStringLiteral("No stream URL set"));
		return;
	}

	// Spotify is played by its own player -- the built-in one or the user's Spotify app --
	// and this only asks it to. There is no connection here to retry, so a refusal is
	// final until the user presses play again.
	if (_kind == Spotify)
	{
		if (_wantsPlayback)
			return;

		_wantsPlayback = true;
		setStatus(Connecting);

		// "spotify:" alone means whatever Spotify last had; anything longer is a URI to
		// start. Once started, play means resume rather than start the playlist over.
		QString	uri = _source == QLatin1String("spotify:") ? QString() : _source;

		_spotify->play(uri == _spotifyStarted ? QString() : uri);
		_spotifyStarted = uri;
		return;
	}

	QUrl	url(_source);

	if (!url.isValid() || url.scheme().isEmpty())
	{
		setStatus(Failed, QStringLiteral("That does not look like a URL"));
		return;
	}

	// Already on its way. A second press is not a restart, though a press while a
	// retry is waiting is taken as "try now".
	if (_wantsPlayback && !_retryPending)
		return;

	_wantsPlayback = true;
	_retryAttempt = 0;

	// A fresh start knows nothing about the server yet.
	_probeConfirmed = false;
	_probeFailures = 0;

	cancelRetry();
	openStream();
}

void	MusicPlayer::pause()
{
	if (!_wantsPlayback)
		return;

	_wantsPlayback = false;

	if (_kind == Spotify)
	{
		_spotify->pause();
		_spotify->setPolling(false);

		setStatus(Paused);
		return;
	}

	_ytDlp->cancel();

	_watchdog.stop();
	cancelRetry();
	_metadata.stop();

	// Stopped underneath rather than paused. A live stream has no position to come
	// back to: un-pausing it later would play stale audio from the buffer and then trip
	// over a connection the server has long since dropped. Resuming reconnects instead,
	// so what comes back is live.
	_player->stop();

	setStatus(Paused);
}

void	MusicPlayer::toggle()
{
	if (active())
		pause();
	else
		play();
}

void	MusicPlayer::stop()
{
	if (_kind == Spotify)
	{
		if (_wantsPlayback)
			_spotify->pause();

		_spotify->setPolling(false);
	}

	_ytDlp->cancel();

	_wantsPlayback = false;
	_retryAttempt = 0;

	_watchdog.stop();
	cancelRetry();

	_metadata.clear();
	_backendTitle.clear();
	_backendStation.clear();

	_player->stop();

	setStatus(Idle);
	updateNowPlaying();
}

void	MusicPlayer::onPlaybackStateChanged()
{
	refreshStatus();
}

void	MusicPlayer::onMediaStatusChanged()
{
	refreshStatus();
}

void	MusicPlayer::onErrorOccurred(QMediaPlayer::Error error, const QString &message)
{
	if (error == QMediaPlayer::NoError)
		return;

	// An error after the user stopped asking is the tail end of that stop.
	if (!_wantsPlayback)
		return;

	scheduleRetry(message.isEmpty() ? QStringLiteral("The stream could not be opened") : message);
}

// Whatever the backend itself can read off the stream. Some backends surface the ICY
// tags here, some nothing at all; either way the ICY reader takes precedence, and this
// only fills in when it has not spoken yet.
void	MusicPlayer::onBackendMetaDataChanged()
{
	// yt-dlp already said what a YouTube source is; whatever the HLS segments carry is
	// less than that.
	if (_kind == YouTube)
		return;

	QMediaMetaData	meta = _player->metaData();

	QString	title = meta.stringValue(QMediaMetaData::Title).trimmed();
	QString	station = meta.stringValue(QMediaMetaData::Publisher).trimmed();

	if (station.isEmpty())
		station = meta.stringValue(QMediaMetaData::AlbumTitle).trimmed();

	if (title == _backendTitle && station == _backendStation)
		return;

	_backendTitle = title;
	_backendStation = station;

	updateNowPlaying();
}

void	MusicPlayer::onWatchdogTimeout()
{
	if (_status != Connecting && _status != Reconnecting)
		return;

	// Nothing arrived and nothing errored. Rather than sit on "Connecting…" forever,
	// drop the attempt and make another.
	scheduleRetry(QStringLiteral("The stream did not start in time"));
}

void	MusicPlayer::onRetryTimeout()
{
	if (!_wantsPlayback)
		return;

	_retryPending = false;
	openStream();
}

// The probe reached the server, so the stream is genuinely there. That both clears any
// run of failures and records that the server allows the second connection, which is
// what lets a later failure be trusted.
void	MusicPlayer::onProbeSucceeded()
{
	_probeConfirmed = true;
	_probeFailures = 0;
}

// The probe could not reach the server. While the player still claims to be playing,
// this is the only honest sign the stream has dropped. It is acted on only once the
// server has proven it tolerates the probe, and only after a couple in a row, so one
// unlucky request does not tear down a stream that is playing fine.
void	MusicPlayer::onProbeFailed()
{
	if (!_wantsPlayback || _status != Playing)
		return;

	if (!_probeConfirmed)
		return;

	if (++_probeFailures < ProbeFailuresForDrop)
		return;

	scheduleRetry(QStringLiteral("The stream stopped responding"));
}

void	MusicPlayer::onReachabilityChanged(QNetworkInformation::Reachability reachability)
{
	// The network is back: no point sitting out the rest of a thirty second wait.
	if (_retryPending && reachability == QNetworkInformation::Reachability::Online)
		_retryTimer.start(NetworkBackRetryMs);

	// The status text talks about the network while it is down, so it needs re-reading.
	if (_status == Reconnecting)
		emit statusChanged();
}

void	MusicPlayer::onMetadataChanged()
{
	updateNowPlaying();
}

void	MusicPlayer::onYouTubeResolved(const QUrl &stream, const QString &title, const QString &channel, bool live,
	const QString &thumbnail)
{
	if (!_wantsPlayback || _retryPending || _kind != YouTube)
		return;

	_backendTitle = title;
	_backendStation = channel;
	_youtubeLive = live;

	if (_youtubeThumbnail.isEmpty())
		_youtubeThumbnail = thumbnail;

	updateNowPlaying();
	startPlayer(stream);
}

// A link that is wrong stays wrong however often it is retried -- a private video, a typo,
// no yt-dlp. Only a failure that looks like the network is worth another attempt.
void	MusicPlayer::onYouTubeFailed(const QString &reason)
{
	if (!_wantsPlayback || _retryPending)
		return;

	bool	transient = networkLooksDown()
		|| reason.contains(QLatin1String("timed out"), Qt::CaseInsensitive)
		|| reason.contains(QLatin1String("Unable to download"), Qt::CaseInsensitive)
		|| reason.contains(QLatin1String("Temporary"), Qt::CaseInsensitive);

	if (transient)
	{
		scheduleRetry(reason);
		return;
	}

	_wantsPlayback = false;
	_watchdog.stop();

	setStatus(Failed, reason);
}

void	MusicPlayer::onSpotifyStarted()
{
	if (!_wantsPlayback || _kind != Spotify)
		return;

	setStatus(Playing);
	_spotify->setPolling(true);
}

void	MusicPlayer::onSpotifyFailed(const QString &reason)
{
	if (!_wantsPlayback || _kind != Spotify)
		return;

	// Nothing started, so the next press should start the chosen playlist again.
	_wantsPlayback = false;
	_spotifyStarted.clear();

	setStatus(Failed, reason);
}

void	MusicPlayer::setStatus(Status status, const QString &errorText)
{
	if (_status == status && _errorText == errorText)
		return;

	_status = status;

	if (_status == Playing)
		_positionTimer.start();
	else
		_positionTimer.stop();

	emit positionChanged();
	_errorText = errorText;

	emit statusChanged();

	updateControls();
}

void	MusicPlayer::refreshStatus()
{
	// An error stands until something is asked of the player again, and while a retry
	// is waiting the player is stopped on purpose, so its state is not news.
	if (_status == Failed || _retryPending)
		return;

	// Not wanted: whatever the player is doing is the tail end of a pause or stop, and
	// the status those set is the one that counts.
	if (!_wantsPlayback)
		return;

	QMediaPlayer::MediaStatus	media = _player->mediaStatus();

	// A YouTube video that is not a live stream has simply finished; start it again, the
	// way a background track is expected to loop.
	if (media == QMediaPlayer::EndOfMedia && _kind == YouTube && !_youtubeLive)
	{
		// In a queue, the end of one video is the start of the next.
		if (_queue.size() > 1)
			moveQueue(1);
		else
			openStream();

		return;
	}

	// A live stream is not supposed to end. When one does, the connection dropped.
	if (media == QMediaPlayer::EndOfMedia)
	{
		scheduleRetry(QStringLiteral("The stream ended"));
		return;
	}

	if (media == QMediaPlayer::InvalidMedia)
	{
		scheduleRetry(QStringLiteral("The stream could not be read"));
		return;
	}

	Status	connecting = _retryAttempt > 0 ? Reconnecting : Connecting;

	if (_player->playbackState() == QMediaPlayer::PlayingState)
	{
		if (media == QMediaPlayer::StalledMedia || media == QMediaPlayer::BufferingMedia
			|| media == QMediaPlayer::LoadingMedia)
		{
			// Also the path a running stream takes when the network goes quiet under it,
			// so the watchdog is re-armed here as well as in openStream().
			if (!_watchdog.isActive())
				_watchdog.start();

			setStatus(connecting);
			return;
		}

		_watchdog.stop();
		_retryAttempt = 0;

		setStatus(Playing);

		// The ICY reader and its liveness probe speak to radio servers. Pointed at a
		// YouTube page they would only download HTML every few seconds.
		if (_kind == Stream)
			_metadata.start(QUrl(_source));

		return;
	}

	// Stopped underneath while still wanted: the attempt is still on its way.
	setStatus(connecting);
}

// A brand new player and output on every connection attempt. Handing the old player a
// new URL is not enough: the FFmpeg backend holds on to the previous stream's buffer and
// never reconnects. Throwing the player away and building a fresh one is what forces a
// genuinely new connection.
//
// The output is rebuilt alongside it rather than reused. A QAudioOutput belongs to one
// player, and destroying the outgoing player would otherwise pull the shared output out
// from under the new one just as it was starting, which is exactly the kind of half-dead
// connection this whole exercise is trying to avoid. Reopening the device on a reconnect
// is cheap next to how rarely reconnects happen.
void	MusicPlayer::rebuildPlayer()
{
	if (_player)
	{
		// Cut its signals first: a player on its way out can still emit a stopped or
		// errored state as it tears down, which must not be mistaken for news about the
		// new one.
		_player->disconnect(this);
		_player->stop();
		_player->setSource(QUrl());
		_player->setAudioOutput(nullptr);
		_player->deleteLater();
	}

	if (_output)
		_output->deleteLater();

	_output = new QAudioOutput(this);
	_output->setVolume(_ducked ? _volume * DuckLevel : _volume);

	_player = new QMediaPlayer(this);
	_player->setAudioOutput(_output);

	connect(_player, &QMediaPlayer::playbackStateChanged, this, &MusicPlayer::onPlaybackStateChanged);
	connect(_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayer::onMediaStatusChanged);
	connect(_player, &QMediaPlayer::errorOccurred, this, &MusicPlayer::onErrorOccurred);
	connect(_player, &QMediaPlayer::durationChanged, this, &MusicPlayer::positionChanged);
	connect(_player, &QMediaPlayer::seekableChanged, this, &MusicPlayer::positionChanged);
	connect(_player, &QMediaPlayer::metaDataChanged, this, &MusicPlayer::onBackendMetaDataChanged);
}

void	MusicPlayer::openStream()
{
	// Each attempt starts the probe count over; whether the server tolerates the probe
	// carries across, since that does not change between one attempt and the next.
	_probeFailures = 0;

	// The reason for the last drop rides along while reconnecting; a fresh start has
	// nothing to explain.
	if (_retryAttempt > 0)
		setStatus(Reconnecting, _errorText);
	else
		setStatus(Connecting);

	// A YouTube link is only a page; yt-dlp finds the audio behind it first, and
	// startPlayer() follows from onYouTubeResolved(). Resolving can take a while the first
	// time yt-dlp runs, so the watchdog gives it longer than a stream gets to start.
	if (_kind == YouTube)
	{
		_watchdog.start(YouTubeResolveMs);
		_ytDlp->resolve(QUrl(_source));
		return;
	}

	startPlayer(QUrl(_source));
}

void	MusicPlayer::startPlayer(const QUrl &url)
{
	// A pristine player, then the URL. This is the reconnect: the old connection and
	// everything it had buffered are gone with the old player.
	rebuildPlayer();
	_player->setSource(url);

	_watchdog.start(WatchdogMs);
	_player->play();
}

void	MusicPlayer::scheduleRetry(const QString &reason)
{
	if (_retryPending || !_wantsPlayback)
		return;

	_retryPending = true;
	_retryAttempt++;

	_watchdog.stop();
	_metadata.stop();

	// Torn down rather than left to its own devices: a backend that is stuck on a dead
	// socket will not recover by itself, and the next attempt wants a clean start.
	_player->stop();

	qInfo("pomodoro: stream dropped (%s), retry %d in %d ms",
		qPrintable(reason), _retryAttempt, retryDelayMs());

	setStatus(Reconnecting, reason);

	_retryTimer.start(retryDelayMs());
}

void	MusicPlayer::cancelRetry()
{
	_retryPending = false;
	_retryTimer.stop();
}

void	MusicPlayer::updateNowPlaying()
{
	emit nowPlayingChanged();

	updateControls();
}

void	MusicPlayer::updateControls()
{
	// Listed with the desktop while the user has any interest in music: playing,
	// trying to, or paused and able to come back. Idle and failed drop the entry.
	//
	// Not for a remote-controlled Spotify: its own app already has the desktop's media
	// controls, and a second entry for the same music would only be confusing. The
	// built-in player has none of its own, so it gets this one.
	bool	enabled = (_wantsPlayback || _status == Paused)
		&& (_kind != Spotify || _spotify->builtInPlayer());

	MediaControls::PlaybackState	state = MediaControls::Stopped;

	// Connecting counts as playing: the user pressed play, so the control they want
	// to see is pause.
	if (_status == Playing || _status == Connecting || _status == Reconnecting)
		state = MediaControls::Playing;
	else if (_status == Paused)
		state = MediaControls::Paused;

	QString	title = this->title();
	QString	station = stationName();
	QString	shownTitle = title;
	QString	shownArtist;

	if (title.isEmpty())
	{
		// Nothing read yet, or a stream that never says. Fall back through the station
		// name to the host, so the desktop never shows an empty entry.
		QString	host = QUrl(_source).host();

		shownTitle = !station.isEmpty() ? station
			: (host.isEmpty() ? QStringLiteral("Lo-fi stream") : host);
	}
	else
		shownArtist = station;

	// Text before state before enabling, so the entry's first appearance is complete.
	_controls->setNowPlaying(shownTitle, shownArtist);
	_controls->setPlaybackState(state);
	_controls->setEnabled(enabled);
}

bool	MusicPlayer::networkLooksDown() const
{
	QNetworkInformation	*network = QNetworkInformation::instance();

	if (!network)
		return false;

	switch (network->reachability())
	{
		case QNetworkInformation::Reachability::Disconnected:
		case QNetworkInformation::Reachability::Local:
		case QNetworkInformation::Reachability::Site:
			return true;
		case QNetworkInformation::Reachability::Online:
		case QNetworkInformation::Reachability::Unknown:
		default:
			return false;
	}
}

int	MusicPlayer::retryDelayMs() const
{
	int	doublings = qBound(0, _retryAttempt - 1, 4);

	return qMin(FirstRetryMs << doublings, MaximumRetryMs);
}
