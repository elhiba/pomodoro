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

	const char *const	KeyAlwaysOnTop = "window/alwaysOnTop";
	const char *const	KeyMinimizeToTray = "window/minimizeToTray";
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

bool	AppSettings::alwaysOnTop() const
{
	return _alwaysOnTop;
}

bool	AppSettings::minimizeToTray() const
{
	return _minimizeToTray;
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

void	AppSettings::setAlwaysOnTop(bool onTop)
{
	if (_alwaysOnTop == onTop)
		return;

	_alwaysOnTop = onTop;
	store(KeyAlwaysOnTop, onTop);

	emit alwaysOnTopChanged();
}

void	AppSettings::setMinimizeToTray(bool toTray)
{
	if (_minimizeToTray == toTray)
		return;

	_minimizeToTray = toTray;
	store(KeyMinimizeToTray, toTray);

	emit minimizeToTrayChanged();
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
	setAlwaysOnTop(DefaultAlwaysOnTop);
	setMinimizeToTray(DefaultMinimizeToTray);
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

	_alwaysOnTop = _store.value(KeyAlwaysOnTop, DefaultAlwaysOnTop).toBool();
	_minimizeToTray = _store.value(KeyMinimizeToTray, DefaultMinimizeToTray).toBool();
}

void	AppSettings::store(const char *key, const QVariant &value)
{
	_store.setValue(QLatin1String(key), value);
}
