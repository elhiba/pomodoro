#include "SoundPlayer.hpp"

#include <QDebug>
#include <QUrl>

namespace
{
	// qt_add_qml_module puts the module's RESOURCES under this prefix. QML reaches them
	// with a relative path, C++ has to spell the whole thing out. If the module URI in
	// CMakeLists.txt ever changes, these two lines change with it.
	const char *const	AlarmSource = "qrc:/qt/qml/Pomodoro/assets/sounds/alarmWood.wav";
	const char *const	ClickSource = "qrc:/qt/qml/Pomodoro/assets/sounds/button.wav";
}

SoundPlayer::SoundPlayer(QObject *parent)
	: QObject(parent)
{
	connect(&_alarm, &QSoundEffect::statusChanged, this, &SoundPlayer::onStatusChanged);
	connect(&_click, &QSoundEffect::statusChanged, this, &SoundPlayer::onStatusChanged);

	// Loading is asynchronous. Setting the sources here rather than on the first play
	// means the samples are decoded long before the first session ends.
	_alarm.setSource(QUrl(QLatin1String(AlarmSource)));
	_click.setSource(QUrl(QLatin1String(ClickSource)));

	applyVolume();
}

qreal	SoundPlayer::volume() const
{
	return _volume;
}

void	SoundPlayer::setVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_volume, volume))
		return;

	_volume = volume;
	applyVolume();

	emit volumeChanged();
}

void	SoundPlayer::playAlarm()
{
	if (qFuzzyIsNull(_volume))
		return;

	_alarm.play();
}

void	SoundPlayer::playClick()
{
	if (qFuzzyIsNull(_volume))
		return;

	// Restarting the sample keeps rapid clicks responsive instead of dropping them.
	_click.stop();
	_click.play();
}

void	SoundPlayer::onStatusChanged()
{
	QSoundEffect	*effect = qobject_cast<QSoundEffect *>(sender());

	if (effect && effect->status() == QSoundEffect::Error)
		qWarning() << "pomodoro: could not load sound" << effect->source();
}

void	SoundPlayer::applyVolume()
{
	_alarm.setVolume(_volume);
	_click.setVolume(_volume * ClickVolumeScale);
}
