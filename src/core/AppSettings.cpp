#include "AppSettings.hpp"

namespace
{
	const char *const	KeyFocusMinutes = "timer/focusMinutes";
	const char *const	KeyShortBreakMinutes = "timer/shortBreakMinutes";
	const char *const	KeyLongBreakMinutes = "timer/longBreakMinutes";
	const char *const	KeyRoundsBeforeLongBreak = "timer/roundsBeforeLongBreak";

	const char *const	KeyAutoStartBreaks = "behaviour/autoStartBreaks";
	const char *const	KeyAutoStartFocus = "behaviour/autoStartFocus";

	const char *const	KeyAlarmVolume = "sound/alarmVolume";

	const char *const	KeyStreamUrl = "music/streamUrl";
	const char *const	KeyMusicVolume = "music/volume";
	const char *const	KeyMusicFollowsFocus = "music/followsFocus";

	const char *const	KeyAlwaysOnTop = "window/alwaysOnTop";
	const char *const	KeyCloseMinimizes = "window/closeMinimizes";
}

const QString	&AppSettings::defaultStreamUrl()
{
	static const QString	url = QStringLiteral("https://stream.zeno.fm/f3wvbbqmdg8uv");

	return url;
}

AppSettings::AppSettings(QObject *parent)
	: QObject(parent)
{
	load();
}

AppSettings::~AppSettings()
{
	// QSettings flushes on destruction anyway, but being explicit means the file is
	// on disk before the rest of the shutdown runs.
	_store.sync();
}

int	AppSettings::focusMinutes() const
{
	return _focusMinutes;
}

int	AppSettings::shortBreakMinutes() const
{
	return _shortBreakMinutes;
}

int	AppSettings::longBreakMinutes() const
{
	return _longBreakMinutes;
}

int	AppSettings::roundsBeforeLongBreak() const
{
	return _roundsBeforeLongBreak;
}

bool	AppSettings::autoStartBreaks() const
{
	return _autoStartBreaks;
}

bool	AppSettings::autoStartFocus() const
{
	return _autoStartFocus;
}

qreal	AppSettings::alarmVolume() const
{
	return _alarmVolume;
}

QString	AppSettings::streamUrl() const
{
	return _streamUrl;
}

qreal	AppSettings::musicVolume() const
{
	return _musicVolume;
}

bool	AppSettings::musicFollowsFocus() const
{
	return _musicFollowsFocus;
}

bool	AppSettings::alwaysOnTop() const
{
	return _alwaysOnTop;
}

bool	AppSettings::closeMinimizes() const
{
	return _closeMinimizes;
}

int	AppSettings::minimumMinutes() const
{
	return MinimumMinutes;
}

int	AppSettings::maximumMinutes() const
{
	return MaximumMinutes;
}

int	AppSettings::minimumRounds() const
{
	return MinimumRounds;
}

int	AppSettings::maximumRounds() const
{
	return MaximumRounds;
}

void	AppSettings::setFocusMinutes(int minutes)
{
	minutes = qBound(MinimumMinutes, minutes, MaximumMinutes);

	if (_focusMinutes == minutes)
		return;

	_focusMinutes = minutes;
	store(KeyFocusMinutes, minutes);

	emit focusMinutesChanged();
}

void	AppSettings::setShortBreakMinutes(int minutes)
{
	minutes = qBound(MinimumMinutes, minutes, MaximumMinutes);

	if (_shortBreakMinutes == minutes)
		return;

	_shortBreakMinutes = minutes;
	store(KeyShortBreakMinutes, minutes);

	emit shortBreakMinutesChanged();
}

void	AppSettings::setLongBreakMinutes(int minutes)
{
	minutes = qBound(MinimumMinutes, minutes, MaximumMinutes);

	if (_longBreakMinutes == minutes)
		return;

	_longBreakMinutes = minutes;
	store(KeyLongBreakMinutes, minutes);

	emit longBreakMinutesChanged();
}

void	AppSettings::setRoundsBeforeLongBreak(int rounds)
{
	rounds = qBound(MinimumRounds, rounds, MaximumRounds);

	if (_roundsBeforeLongBreak == rounds)
		return;

	_roundsBeforeLongBreak = rounds;
	store(KeyRoundsBeforeLongBreak, rounds);

	emit roundsBeforeLongBreakChanged();
}

void	AppSettings::setAutoStartBreaks(bool autoStart)
{
	if (_autoStartBreaks == autoStart)
		return;

	_autoStartBreaks = autoStart;
	store(KeyAutoStartBreaks, autoStart);

	emit autoStartBreaksChanged();
}

void	AppSettings::setAutoStartFocus(bool autoStart)
{
	if (_autoStartFocus == autoStart)
		return;

	_autoStartFocus = autoStart;
	store(KeyAutoStartFocus, autoStart);

	emit autoStartFocusChanged();
}

void	AppSettings::setAlarmVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_alarmVolume, volume))
		return;

	_alarmVolume = volume;
	store(KeyAlarmVolume, volume);

	emit alarmVolumeChanged();
}

void	AppSettings::setStreamUrl(const QString &url)
{
	QString	trimmed = url.trimmed();

	if (_streamUrl == trimmed)
		return;

	_streamUrl = trimmed;
	store(KeyStreamUrl, trimmed);

	emit streamUrlChanged();
}

void	AppSettings::setMusicVolume(qreal volume)
{
	volume = qBound(0.0, volume, 1.0);

	if (qFuzzyCompare(_musicVolume, volume))
		return;

	_musicVolume = volume;
	store(KeyMusicVolume, volume);

	emit musicVolumeChanged();
}

void	AppSettings::setMusicFollowsFocus(bool follows)
{
	if (_musicFollowsFocus == follows)
		return;

	_musicFollowsFocus = follows;
	store(KeyMusicFollowsFocus, follows);

	emit musicFollowsFocusChanged();
}

void	AppSettings::setAlwaysOnTop(bool onTop)
{
	if (_alwaysOnTop == onTop)
		return;

	_alwaysOnTop = onTop;
	store(KeyAlwaysOnTop, onTop);

	emit alwaysOnTopChanged();
}

void	AppSettings::setCloseMinimizes(bool minimizes)
{
	if (_closeMinimizes == minimizes)
		return;

	_closeMinimizes = minimizes;
	store(KeyCloseMinimizes, minimizes);

	emit closeMinimizesChanged();
}

void	AppSettings::restoreDefaults()
{
	setFocusMinutes(DefaultFocusMinutes);
	setShortBreakMinutes(DefaultShortBreakMinutes);
	setLongBreakMinutes(DefaultLongBreakMinutes);
	setRoundsBeforeLongBreak(DefaultRoundsBeforeLongBreak);
	setAutoStartBreaks(DefaultAutoStartBreaks);
	setAutoStartFocus(DefaultAutoStartFocus);
	setAlarmVolume(DefaultAlarmVolume);
	setStreamUrl(defaultStreamUrl());
	setMusicVolume(DefaultMusicVolume);
	setMusicFollowsFocus(DefaultMusicFollowsFocus);
	setAlwaysOnTop(DefaultAlwaysOnTop);
	setCloseMinimizes(DefaultCloseMinimizes);
}

void	AppSettings::load()
{
	// Values are clamped on the way in as well as on the way out, so a hand edited
	// or corrupted config file cannot put the timer into a nonsense state.
	_focusMinutes = qBound(MinimumMinutes,
		_store.value(KeyFocusMinutes, DefaultFocusMinutes).toInt(), MaximumMinutes);

	_shortBreakMinutes = qBound(MinimumMinutes,
		_store.value(KeyShortBreakMinutes, DefaultShortBreakMinutes).toInt(), MaximumMinutes);

	_longBreakMinutes = qBound(MinimumMinutes,
		_store.value(KeyLongBreakMinutes, DefaultLongBreakMinutes).toInt(), MaximumMinutes);

	_roundsBeforeLongBreak = qBound(MinimumRounds,
		_store.value(KeyRoundsBeforeLongBreak, DefaultRoundsBeforeLongBreak).toInt(), MaximumRounds);

	_autoStartBreaks = _store.value(KeyAutoStartBreaks, DefaultAutoStartBreaks).toBool();
	_autoStartFocus = _store.value(KeyAutoStartFocus, DefaultAutoStartFocus).toBool();

	_alarmVolume = qBound(0.0, _store.value(KeyAlarmVolume, DefaultAlarmVolume).toDouble(), 1.0);

	_streamUrl = _store.value(KeyStreamUrl, defaultStreamUrl()).toString().trimmed();
	_musicVolume = qBound(0.0, _store.value(KeyMusicVolume, DefaultMusicVolume).toDouble(), 1.0);
	_musicFollowsFocus = _store.value(KeyMusicFollowsFocus, DefaultMusicFollowsFocus).toBool();

	_alwaysOnTop = _store.value(KeyAlwaysOnTop, DefaultAlwaysOnTop).toBool();
	_closeMinimizes = _store.value(KeyCloseMinimizes, DefaultCloseMinimizes).toBool();
}

void	AppSettings::store(const char *key, const QVariant &value)
{
	_store.setValue(QLatin1String(key), value);
}
