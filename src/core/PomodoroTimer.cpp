#include "PomodoroTimer.hpp"

PomodoroTimer::PomodoroTimer(QObject *parent)
	: QObject(parent)
{
	_tickTimer.setInterval(TickIntervalMs);
	_tickTimer.setTimerType(Qt::PreciseTimer);

	connect(&_tickTimer, &QTimer::timeout, this, &PomodoroTimer::onTick);

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

void	PomodoroTimer::setFocusMinutes(int minutes)
{
	minutes = qMax(1, minutes);

	if (_focusMinutes == minutes)
		return;

	_focusMinutes = minutes;
	emit focusMinutesChanged();

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

	// Starting from a session that already ran out means starting it over.
	if (_remainingMs <= 0)
		applyMode(_mode);

	_elapsed.start();
	_tickTimer.start();

	setState(Running);
	refresh();
}

void	PomodoroTimer::pause()
{
	if (_state != Running)
		return;

	_consumedMs += _elapsed.elapsed();
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
	_tickTimer.stop();

	setState(Idle);
	applyMode(_mode);
}

void	PomodoroTimer::skip()
{
	// A skipped session was not completed, so it does not count towards a long break.
	setMode(nextMode());
}

void	PomodoroTimer::setMode(Mode mode)
{
	_tickTimer.stop();
	setState(Idle);

	if (_mode != mode)
	{
		_mode = mode;
		emit modeChanged();
	}

	applyMode(mode);
}

void	PomodoroTimer::onTick()
{
	refresh();

	if (_remainingMs <= 0)
		finishSession();
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

void	PomodoroTimer::applyMode(Mode mode)
{
	qint64	total = static_cast<qint64>(minutesFor(mode)) * 60 * 1000;

	_consumedMs = 0;

	if (_totalMs != total)
	{
		_totalMs = total;
		emit totalSecondsChanged();
	}

	refresh();
}

void	PomodoroTimer::refresh()
{
	qint64	consumed = _consumedMs;

	if (_state == Running)
		consumed += _elapsed.elapsed();

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

void	PomodoroTimer::finishSession()
{
	Mode	finished = _mode;

	_tickTimer.stop();

	if (finished == Focus)
	{
		_completedRounds++;
		emit completedRoundsChanged();
	}

	Mode	next = nextMode();

	setMode(next);

	emit sessionFinished(finished, next);
}
