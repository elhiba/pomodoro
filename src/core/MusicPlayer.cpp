#include "MusicPlayer.hpp"

#include <QDebug>

MusicPlayer::MusicPlayer(QObject *parent)
	: QObject(parent)
{
	_player.setAudioOutput(&_output);
	_output.setVolume(_volume);

	_watchdog.setSingleShot(true);
	_watchdog.setInterval(WatchdogMs);

	connect(&_player, &QMediaPlayer::playbackStateChanged, this, &MusicPlayer::onPlaybackStateChanged);
	connect(&_player, &QMediaPlayer::mediaStatusChanged, this, &MusicPlayer::onMediaStatusChanged);
	connect(&_player, &QMediaPlayer::errorOccurred, this, &MusicPlayer::onErrorOccurred);
	connect(&_watchdog, &QTimer::timeout, this, &MusicPlayer::onWatchdogTimeout);
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
		case Failed:
			return _errorText.isEmpty() ? QStringLiteral("Could not play the stream") : _errorText;
		case Idle:
		default:
			return _source.isEmpty() ? QStringLiteral("No stream set") : QStringLiteral("Stopped");
	}
}

bool	MusicPlayer::active() const
{
	return _status == Connecting || _status == Playing;
}

bool	MusicPlayer::failed() const
{
	return _status == Failed;
}

void	MusicPlayer::setSource(const QString &source)
{
	QString	trimmed = source.trimmed();

	if (_source == trimmed)
		return;

	_source = trimmed;

	// Changing the stream out from under a playing one starts the new one instead.
	bool	wasPlaying = _wantsPlayback;

	stop();

	// Deliberately NOT handed to QMediaPlayer here. The FFmpeg backend opens the URL
	// as soon as it is set, to probe the format, which would connect to the stream on
	// every launch before the user has asked for any music. play() sets it instead.
	_player.setSource(QUrl());

	emit sourceChanged();

	if (wasPlaying && !_source.isEmpty())
		play();
}

void	MusicPlayer::setVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_volume, volume))
		return;

	_volume = volume;
	_output.setVolume(_volume);

	emit volumeChanged();
}

void	MusicPlayer::play()
{
	if (_source.isEmpty())
	{
		setStatus(Failed, QStringLiteral("No stream URL set"));
		return;
	}

	if (!QUrl(_source).isValid() || QUrl(_source).scheme().isEmpty())
	{
		setStatus(Failed, QStringLiteral("That does not look like a URL"));
		return;
	}

	if (_player.source() != QUrl(_source))
		_player.setSource(QUrl(_source));

	_wantsPlayback = true;

	// A live stream has no position to resume from, so a failed attempt is retried
	// from scratch rather than un-paused.
	if (_status == Failed)
		_player.setSource(QUrl(_source));

	setStatus(Connecting);
	_watchdog.start();

	_player.play();
}

void	MusicPlayer::pause()
{
	if (!_wantsPlayback)
		return;

	_wantsPlayback = false;
	_watchdog.stop();

	_player.pause();

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
	_wantsPlayback = false;
	_watchdog.stop();

	_player.stop();

	setStatus(Idle);
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

	_wantsPlayback = false;
	_watchdog.stop();

	setStatus(Failed, message.isEmpty() ? QStringLiteral("The stream could not be opened") : message);
}

void	MusicPlayer::onWatchdogTimeout()
{
	if (_status != Connecting)
		return;

	// Nothing arrived and nothing errored. Rather than sit on "Connecting…" forever,
	// give up and say so.
	_wantsPlayback = false;
	_player.stop();

	setStatus(Failed, QStringLiteral("The stream did not start in time"));
}

void	MusicPlayer::setStatus(Status status, const QString &errorText)
{
	if (_status == status && _errorText == errorText)
		return;

	_status = status;
	_errorText = errorText;

	emit statusChanged();
}

void	MusicPlayer::refreshStatus()
{
	// An error stands until something is asked of the player again.
	if (_status == Failed)
		return;

	// A live stream is not supposed to end. When one does, the connection dropped, and
	// saying so beats falling through to the stopped branch below and sitting on
	// "Connecting…" forever with the watchdog already switched off.
	if (_player.mediaStatus() == QMediaPlayer::EndOfMedia)
	{
		if (_wantsPlayback)
		{
			_wantsPlayback = false;
			_watchdog.stop();

			setStatus(Failed, QStringLiteral("The stream ended"));
		}
		else
			setStatus(Idle);

		return;
	}

	if (_player.playbackState() == QMediaPlayer::PlayingState)
	{
		QMediaPlayer::MediaStatus	media = _player.mediaStatus();

		if (media == QMediaPlayer::StalledMedia || media == QMediaPlayer::BufferingMedia
			|| media == QMediaPlayer::LoadingMedia)
		{
			setStatus(Connecting);
			return;
		}

		_watchdog.stop();
		setStatus(Playing);
		return;
	}

	if (_player.playbackState() == QMediaPlayer::PausedState)
	{
		setStatus(Paused);
		return;
	}

	// Stopped. Still connecting counts as wanted, anything else is simply idle.
	if (_wantsPlayback)
		setStatus(Connecting);
	else
		setStatus(Idle);
}
