#ifndef SOUND_PLAYER_HPP
#define SOUND_PLAYER_HPP

#include <QObject>
#include <QSoundEffect>

#include <QtQml/qqmlregistration.h>

// The short audio cues. QSoundEffect rather than QMediaPlayer on purpose: these are
// small uncompressed WAVs that have to fire the instant they are asked for, which is
// exactly what QSoundEffect is for. QMediaPlayer would add decoding latency.
//
// Registered as a singleton so the buttons scattered around the UI can ask for a click
// without the player being threaded down through every component.
class SoundPlayer : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(qreal volume READ volume WRITE setVolume NOTIFY volumeChanged)

	public:
		explicit SoundPlayer(QObject *parent = nullptr);

		qreal	volume() const;
		void	setVolume(qreal volume);

	public slots:
		void	playAlarm();
		void	playClick();

	signals:
		void	volumeChanged();

	private slots:
		void	onStatusChanged();

	private:
		// A click is a background cue rather than an event, so it sits below the alarm.
		static constexpr qreal	ClickVolumeScale = 0.5;

		QSoundEffect	_alarm;
		QSoundEffect	_click;

		qreal	_volume = 0.7;

		void	applyVolume();
};

#endif
