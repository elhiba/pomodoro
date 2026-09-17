#include "PomodoroTimer.hpp"

PomodoroTimer::PomodoroTimer(QObject *parent)
	: QObject(parent)
{
	_tickTimer.setInterval(TickIntervalMs);
	_tickTimer.setTimerType(Qt::PreciseTimer);

	connect(&_tickTimer, &QTimer::timeout, this, &PomodoroTimer::onTick);

	initSession(Focus);
	initSession(ShortBreak);
	initSession(LongBreak);

	loadSession(_mode);
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

	// Only a session that has not started yet may be re-lengthened under the user.
	if (_sessions[Focus].state == Idle)
	{
		initSession(Focus);
		if (_mode == Focus)
			loadSession(Focus);
	}
}

void	PomodoroTimer::setShortBreakMinutes(int minutes)
{
	minutes = qMax(1, minutes);

	if (_shortBreakMinutes == minutes)
		return;

	_shortBreakMinutes = minutes;
	emit shortBreakMinutesChanged();

	if (_sessions[ShortBreak].state == Idle)
	{
		initSession(ShortBreak);
		if (_mode == ShortBreak)
			loadSession(ShortBreak);
	}
}

void	PomodoroTimer::setLongBreakMinutes(int minutes)
{
	minutes = qMax(1, minutes);

	if (_longBreakMinutes == minutes)
		return;

	_longBreakMinutes = minutes;
	emit longBreakMinutesChanged();

	if (_sessions[LongBreak].state == Idle)
	{
		initSession(LongBreak);
		if (_mode == LongBreak)
			loadSession(LongBreak);
	}
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
	SessionState	&s = _sessions[_mode];

	if (s.state == Running)
		return;

	// Starting from a session that already ran out means starting it over.
	if (s.remainingMs <= 0)
		initSession(_mode);

	for (int m = 0; m < 3; m++)
	{
		if (m != _mode && _sessions[m].state == Running)
			initSession(static_cast<Mode>(m));
	}

	s.elapsed.start();
	s.state = Running;

	_tickTimer.start();

	loadSession(_mode);
}

void	PomodoroTimer::pause()
{
	SessionState	&s = _sessions[_mode];

	if (s.state != Running)
		return;

	s.consumedMs += s.elapsed.elapsed();
	s.state = Paused;

	bool anyRunning = false;
	for (int m = 0; m < 3; m++)
	{
		if (_sessions[m].state == Running)
			anyRunning = true;
	}
	if (!anyRunning)
		_tickTimer.stop();

	loadSession(_mode);
}

void	PomodoroTimer::toggle()
{
	if (_sessions[_mode].state == Running)
		pause();
	else
		start();
}

void	PomodoroTimer::reset()
{
	initSession(_mode);

	bool anyRunning = false;
	for (int m = 0; m < 3; m++)
	{
		if (_sessions[m].state == Running)
			anyRunning = true;
	}
	if (!anyRunning)
		_tickTimer.stop();

	loadSession(_mode);
}

void	PomodoroTimer::skip()
{
	// A skipped session was not completed, so it does not count towards a long break.
	initSession(_mode);
	setMode(nextMode());
}

void	PomodoroTimer::setMode(Mode mode)
{
	if (_mode == mode)
		return;

	_mode = mode;
	emit modeChanged();

	loadSession(mode);
}

void	PomodoroTimer::onTick()
{
	for (int m = 0; m < 3; m++)
	{
		SessionState	&s = _sessions[m];
		if (s.state == Running)
		{
			qint64 consumed = s.consumedMs + s.elapsed.elapsed();
			s.remainingMs = qMax<qint64>(0, s.totalMs - consumed);
			s.remainingSeconds = static_cast<int>((s.remainingMs + 999) / 1000);

			if (s.remainingMs <= 0)
			{
				finishSession(static_cast<Mode>(m));
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
	_sessions[_mode].state = state;
	emit stateChanged();
}

void	PomodoroTimer::initSession(Mode mode)
{
	qint64	total = static_cast<qint64>(minutesFor(mode)) * 60 * 1000;
	SessionState	&s = _sessions[mode];
	s.state = Idle;
	s.totalMs = total;
	s.consumedMs = 0;
	s.remainingMs = total;
	s.remainingSeconds = static_cast<int>((total + 999) / 1000);
}

void	PomodoroTimer::loadSession(Mode mode)
{
	SessionState	&s = _sessions[mode];
	setState(s.state);

	if (_totalMs != s.totalMs)
	{
		_totalMs = s.totalMs;
		emit totalSecondsChanged();
	}

	_consumedMs = s.consumedMs;

	if (_remainingMs != s.remainingMs)
	{
		_remainingMs = s.remainingMs;
		emit progressChanged();
	}

	if (_remainingSeconds != s.remainingSeconds)
	{
		_remainingSeconds = s.remainingSeconds;
		emit remainingSecondsChanged();
		emit displayTimeChanged();
	}
}

void	PomodoroTimer::applyMode(Mode mode)
{
	initSession(mode);
	if (_mode == mode)
		loadSession(mode);
}

void	PomodoroTimer::refresh()
{
	const SessionState	&s = _sessions[_mode];

	if (_remainingMs != s.remainingMs)
	{
		_remainingMs = s.remainingMs;
		emit progressChanged();
	}

	// Round up, so a fresh 25 minute session reads 25:00 rather than 24:59
	// and the display only reaches 00:00 when the session is genuinely over.
	int	seconds = static_cast<int>((s.remainingMs + 999) / 1000);

	if (_remainingSeconds != seconds)
	{
		_remainingSeconds = seconds;
		emit remainingSecondsChanged();
		emit displayTimeChanged();
	}
}

void	PomodoroTimer::finishSession()
{
	finishSession(_mode);
}

void	PomodoroTimer::finishSession(Mode finished)
{
	// Read before setMode re-arms the clock for the next session.
	int		durationSeconds = static_cast<int>(_sessions[finished].totalMs / 1000);

	if (finished == Focus)
	{
		_completedRounds++;
		emit completedRoundsChanged();
	}

	Mode	next = nextMode();

	initSession(finished);
	initSession(next);

	setMode(next);

	// Announced before anything auto starts, so a listener sees the finished session
	// settled at the top of the next one rather than already counting down.
	emit sessionFinished(finished, next, durationSeconds);

	bool	autoStart = (next == Focus) ? _autoStartFocus : _autoStartBreaks;

	if (autoStart)
		start();
	else
	{
		bool anyRunning = false;
		for (int m = 0; m < 3; m++)
		{
			if (_sessions[m].state == Running)
				anyRunning = true;
		}
		if (!anyRunning)
			_tickTimer.stop();
	}
}
