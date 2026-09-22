#include "PomodoroTimer.hpp"

PomodoroTimer::PomodoroTimer(QObject *parent)
	: QObject(parent)
{
	_tickTimer.setInterval(TickIntervalMs);
	_tickTimer.setTimerType(Qt::PreciseTimer);

	connect(&_tickTimer, &QTimer::timeout, this, &PomodoroTimer::onTick);

	for (int i = 0; i < 3; ++i)
	{
		_modeStates[i].state = Idle;
		_modeStates[i].totalMs = static_cast<qint64>(minutesFor(static_cast<Mode>(i))) * 60 * 1000;
		_modeStates[i].consumedMs = 0;
	}

	applyMode(_mode);
}

PomodoroTimer::Mode	PomodoroTimer::mode() const
{
	return _mode;
}

PomodoroTimer::State	PomodoroTimer::state() const
{
	return _state;
}

int	PomodoroTimer::remainingSeconds() const
{
	return _remainingSeconds;
}

int	PomodoroTimer::totalSeconds() const
{
	return static_cast<int>(_totalMs / 1000);
}

QString	PomodoroTimer::displayTime() const
{
	int	minutes = _remainingSeconds / 60;
	int	seconds = _remainingSeconds % 60;

	return QStringLiteral("%1:%2")
		.arg(minutes, 2, 10, QLatin1Char('0'))
		.arg(seconds, 2, 10, QLatin1Char('0'));
}

qreal	PomodoroTimer::progress() const
{
	if (_totalMs <= 0)
		return 0.0;

	return static_cast<qreal>(_remainingMs) / static_cast<qreal>(_totalMs);
}

int	PomodoroTimer::completedRounds() const
{
	return _completedRounds;
}

int	PomodoroTimer::focusMinutes() const
{
	return _focusMinutes;
}

int	PomodoroTimer::shortBreakMinutes() const
{
	return _shortBreakMinutes;
}

int	PomodoroTimer::longBreakMinutes() const
{
	return _longBreakMinutes;
}

int	PomodoroTimer::roundsBeforeLongBreak() const
{
	return _roundsBeforeLongBreak;
}

bool	PomodoroTimer::autoStartBreaks() const
{
	return _autoStartBreaks;
}

bool	PomodoroTimer::autoStartFocus() const
{
	return _autoStartFocus;
}

void	PomodoroTimer::setAutoStartBreaks(bool autoStart)
{
	if (_autoStartBreaks == autoStart)
		return;

	_autoStartBreaks = autoStart;
	emit autoStartBreaksChanged();
}

void	PomodoroTimer::setAutoStartFocus(bool autoStart)
{
	if (_autoStartFocus == autoStart)
		return;

	_autoStartFocus = autoStart;
	emit autoStartFocusChanged();
}

void	PomodoroTimer::setFocusMinutes(int minutes)
{
	minutes = qMax(1, minutes);

	if (_focusMinutes == minutes)
		return;

	_focusMinutes = minutes;
	emit focusMinutesChanged();

	if (_modeStates[Focus].state == Idle)
		_modeStates[Focus].totalMs = static_cast<qint64>(minutes) * 60 * 1000;

	// Only a session that has not started yet may be re-lengthened under the user.
	if (_mode == Focus && _state == Idle)
		applyMode(Focus);
}

void	PomodoroTimer::setShortBreakMinutes(int minutes)
{
	minutes = qMax(1, minutes);

	if (_shortBreakMinutes == minutes)
		return;

	_shortBreakMinutes = minutes;
	emit shortBreakMinutesChanged();

	if (_modeStates[ShortBreak].state == Idle)
		_modeStates[ShortBreak].totalMs = static_cast<qint64>(minutes) * 60 * 1000;

	if (_mode == ShortBreak && _state == Idle)
		applyMode(ShortBreak);
}

void	PomodoroTimer::setLongBreakMinutes(int minutes)
{
	minutes = qMax(1, minutes);

	if (_longBreakMinutes == minutes)
		return;

	_longBreakMinutes = minutes;
	emit longBreakMinutesChanged();

	if (_modeStates[LongBreak].state == Idle)
		_modeStates[LongBreak].totalMs = static_cast<qint64>(minutes) * 60 * 1000;

	if (_mode == LongBreak && _state == Idle)
		applyMode(LongBreak);
}

void	PomodoroTimer::setRoundsBeforeLongBreak(int rounds)
{
	rounds = qMax(1, rounds);

	if (_roundsBeforeLongBreak == rounds)
		return;

	_roundsBeforeLongBreak = rounds;
	emit roundsBeforeLongBreakChanged();
}

void	PomodoroTimer::start()
{
	if (_state == Running)
		return;

	for (int i = 0; i < 3; ++i)
	{
		if (i != _mode && _modeStates[i].state == Running)
		{
			_modeStates[i].state = Idle;
			_modeStates[i].consumedMs = 0;
		}
	}

	if (_remainingMs <= 0)
		applyMode(_mode);

	_modeStates[_mode].elapsed.start();
	_modeStates[_mode].state = Running;

	_tickTimer.start();

	setState(Running);
	refresh();
}

void	PomodoroTimer::pause()
{
	if (_state != Running)
		return;

	_modeStates[_mode].consumedMs += _modeStates[_mode].elapsed.elapsed();
	_modeStates[_mode].state = Paused;

	bool	anyRunning = false;
	for (int i = 0; i < 3; ++i)
	{
		if (_modeStates[i].state == Running)
			anyRunning = true;
	}

	if (!anyRunning)
		_tickTimer.stop();

	setState(Paused);
	refresh();
}

void	PomodoroTimer::toggle()
{
	if (_state == Running)
		pause();
	else
		start();
}

void	PomodoroTimer::reset()
{
	_modeStates[_mode].state = Idle;
	_modeStates[_mode].consumedMs = 0;

	bool	anyRunning = false;
	for (int i = 0; i < 3; ++i)
	{
		if (_modeStates[i].state == Running)
			anyRunning = true;
	}

	if (!anyRunning)
		_tickTimer.stop();

	applyMode(_mode);
}

void	PomodoroTimer::skip()
{
	Mode	next = nextMode();

	_modeStates[_mode].state = Idle;
	_modeStates[_mode].consumedMs = 0;

	_modeStates[next].state = Idle;
	_modeStates[next].consumedMs = 0;
	_modeStates[next].totalMs = static_cast<qint64>(minutesFor(next)) * 60 * 1000;

	bool	anyRunning = false;
	for (int i = 0; i < 3; ++i)
	{
		if (_modeStates[i].state == Running)
			anyRunning = true;
	}

	if (!anyRunning)
		_tickTimer.stop();

	_mode = next;
	emit modeChanged();

	applyMode(next);
}

void	PomodoroTimer::setMode(Mode mode)
{
	if (_mode == mode)
		return;

	_mode = mode;
	emit modeChanged();

	restoreMode(mode);
}

void	PomodoroTimer::onTick()
{
	for (int i = 0; i < 3; ++i)
	{
		if (_modeStates[i].state == Running)
		{
			qint64	consumed = _modeStates[i].consumedMs + _modeStates[i].elapsed.elapsed();

			if (consumed >= _modeStates[i].totalMs)
			{
				finishSession(static_cast<Mode>(i));
				return;
			}
		}
	}

	refresh();
}

int	PomodoroTimer::minutesFor(Mode mode) const
{
	switch (mode)
	{
		case ShortBreak:
			return _shortBreakMinutes;
		case LongBreak:
			return _longBreakMinutes;
		case Focus:
		default:
			return _focusMinutes;
	}
}

PomodoroTimer::Mode	PomodoroTimer::nextMode() const
{
	if (_mode != Focus)
		return Focus;

	if (_completedRounds > 0 && _completedRounds % _roundsBeforeLongBreak == 0)
		return LongBreak;

	return ShortBreak;
}

void	PomodoroTimer::setState(State state)
{
	if (_state == state)
		return;

	_state = state;
	emit stateChanged();
}

void	PomodoroTimer::restoreMode(Mode mode)
{
	ModeState	&ms = _modeStates[mode];

	if (ms.state == Idle)
	{
		applyMode(mode);
		return;
	}

	if (_totalMs != ms.totalMs)
	{
		_totalMs = ms.totalMs;
		emit totalSecondsChanged();
	}

	setState(ms.state);
	refresh();
}

void	PomodoroTimer::applyMode(Mode mode)
{
	qint64	total = static_cast<qint64>(minutesFor(mode)) * 60 * 1000;

	_modeStates[mode].totalMs = total;
	_modeStates[mode].consumedMs = 0;
	_modeStates[mode].state = Idle;

	if (_totalMs != total)
	{
		_totalMs = total;
		emit totalSecondsChanged();
	}

	setState(Idle);
	refresh();
}

void	PomodoroTimer::refresh()
{
	qint64	consumed = _modeStates[_mode].consumedMs;

	if (_modeStates[_mode].state == Running)
		consumed += _modeStates[_mode].elapsed.elapsed();

	qint64	remaining = qMax<qint64>(0, _totalMs - consumed);

	if (_remainingMs != remaining)
	{
		_remainingMs = remaining;
		emit progressChanged();
	}

	// Round up, so a fresh 25 minute session reads 25:00 rather than 24:59
	// and the display only reaches 00:00 when the session is genuinely over.
	int	seconds = static_cast<int>((remaining + 999) / 1000);

	if (_remainingSeconds != seconds)
	{
		_remainingSeconds = seconds;
		emit remainingSecondsChanged();
		emit displayTimeChanged();
	}
}

void	PomodoroTimer::finishSession(Mode finished)
{
	int		durationSeconds = static_cast<int>(_modeStates[finished].totalMs / 1000);

	_modeStates[finished].state = Idle;
	_modeStates[finished].consumedMs = 0;

	if (finished == Focus)
	{
		_completedRounds++;
		emit completedRoundsChanged();
	}

	Mode	next = nextMode();
	bool	autoStart = (next == Focus) ? _autoStartFocus : _autoStartBreaks;

	if (_mode == finished)
	{
		_mode = next;
		emit modeChanged();

		applyMode(next);

		emit sessionFinished(finished, next, durationSeconds);

		if (autoStart)
			start();
	}
	else
	{
		_modeStates[next].state = Idle;
		_modeStates[next].totalMs = static_cast<qint64>(minutesFor(next)) * 60 * 1000;
		_modeStates[next].consumedMs = 0;

		emit sessionFinished(finished, next, durationSeconds);

		if (autoStart)
		{
			_modeStates[next].elapsed.start();
			_modeStates[next].state = Running;
		}

		bool	anyRunning = false;
		for (int i = 0; i < 3; ++i)
		{
			if (_modeStates[i].state == Running)
				anyRunning = true;
		}

		if (!anyRunning)
			_tickTimer.stop();

		refresh();
	}
}
