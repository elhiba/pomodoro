#ifndef MUSIC_PLAYER_HPP
#define MUSIC_PLAYER_HPP

#include <QAudioOutput>
#include <QMediaPlayer>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>

#include <QtQml/qqmlregistration.h>

// The background stream. QMediaPlayer here rather than QSoundEffect: this is a
// compressed network stream of unbounded length, the opposite of the short PCM cues
// SoundPlayer handles.
//
// The whole point of the status property is that a stream which will not start has to
// say so. A dead URL must not leave the UI sitting on a play icon forever, so there is
// a watchdog as well as the usual error signal.
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

	public:
		enum Status
		{
			Idle,
			Connecting,
			Playing,
			Paused,
			Failed
		};
		Q_ENUM(Status)

		explicit MusicPlayer(QObject *parent = nullptr);

		QString	source() const;
		qreal	volume() const;
		Status	status() const;
		QString	statusText() const;

		// True while the user has asked for music, whether or not a note has arrived yet.
		bool	active() const;
		bool	failed() const;

		void	setSource(const QString &source);
		void	setVolume(qreal volume);

	public slots:
		void	play();
		void	pause();
		void	toggle();
		void	stop();

	signals:
		void	sourceChanged();
		void	volumeChanged();
		void	statusChanged();

	private slots:
		void	onPlaybackStateChanged();
		void	onMediaStatusChanged();
		void	onErrorOccurred(QMediaPlayer::Error error, const QString &message);
		void	onWatchdogTimeout();

	private:
		// Long enough for a slow connection to a live stream, short enough that a dead
		// host does not leave the UI pretending it is about to work.
		static constexpr int	WatchdogMs = 20000;

		QMediaPlayer	_player;
		QAudioOutput	_output;
		QTimer			_watchdog;

		QString	_source;
		qreal	_volume = 0.5;
		Status	_status = Idle;
		QString	_errorText;

		// What the user asked for, as opposed to what the network is currently doing.
		bool	_wantsPlayback = false;

		void	setStatus(Status status, const QString &errorText = QString());
		void	refreshStatus();
};

#endif
