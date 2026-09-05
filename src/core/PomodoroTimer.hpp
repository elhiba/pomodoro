#ifndef POMODORO_TIMER_HPP
#define POMODORO_TIMER_HPP

#include <QObject>
#include <QElapsedTimer>
#include <QString>
#include <QTimer>

#include <QtQml/qqmlregistration.h>

// Owns the whole session state machine: how long the current session lasts,
// how much of it is left, and which session comes next. The QML side only
// binds to the properties below and calls the slots, it never counts anything
// by itself.
class PomodoroTimer : public QObject
{
	Q_OBJECT
	QML_ELEMENT

	Q_PROPERTY(Mode mode READ mode NOTIFY modeChanged)
	Q_PROPERTY(State state READ state NOTIFY stateChanged)

	Q_PROPERTY(int remainingSeconds READ remainingSeconds NOTIFY remainingSecondsChanged)
	Q_PROPERTY(int totalSeconds READ totalSeconds NOTIFY totalSecondsChanged)
	Q_PROPERTY(QString displayTime READ displayTime NOTIFY displayTimeChanged)
	Q_PROPERTY(qreal progress READ progress NOTIFY progressChanged)
	Q_PROPERTY(int completedRounds READ completedRounds NOTIFY completedRoundsChanged)

	Q_PROPERTY(int focusMinutes READ focusMinutes WRITE setFocusMinutes NOTIFY focusMinutesChanged)
	Q_PROPERTY(int shortBreakMinutes READ shortBreakMinutes WRITE setShortBreakMinutes NOTIFY shortBreakMinutesChanged)
	Q_PROPERTY(int longBreakMinutes READ longBreakMinutes WRITE setLongBreakMinutes NOTIFY longBreakMinutesChanged)
	Q_PROPERTY(int roundsBeforeLongBreak READ roundsBeforeLongBreak WRITE setRoundsBeforeLongBreak NOTIFY roundsBeforeLongBreakChanged)

	Q_PROPERTY(bool autoStartBreaks READ autoStartBreaks WRITE setAutoStartBreaks NOTIFY autoStartBreaksChanged)
	Q_PROPERTY(bool autoStartFocus READ autoStartFocus WRITE setAutoStartFocus NOTIFY autoStartFocusChanged)

	public:
		enum Mode
		{
			Focus,
			ShortBreak,
			LongBreak
		};
		Q_ENUM(Mode)

		enum State
		{
			Idle,
			Running,
			Paused
		};
		Q_ENUM(State)

		explicit PomodoroTimer(QObject *parent = nullptr);

		Mode	mode() const;
		State	state() const;

		int		remainingSeconds() const;
		int		totalSeconds() const;
		QString	displayTime() const;
		qreal	progress() const;
		int		completedRounds() const;

		int		focusMinutes() const;
		int		shortBreakMinutes() const;
		int		longBreakMinutes() const;
		int		roundsBeforeLongBreak() const;
		bool	autoStartBreaks() const;
		bool	autoStartFocus() const;

		void	setFocusMinutes(int minutes);
		void	setShortBreakMinutes(int minutes);
		void	setLongBreakMinutes(int minutes);
		void	setRoundsBeforeLongBreak(int rounds);
		void	setAutoStartBreaks(bool autoStart);
		void	setAutoStartFocus(bool autoStart);

	public slots:
		void	start();
		void	pause();
		void	toggle();
		void	reset();
		void	skip();
		void	setMode(Mode mode);

	signals:
		void	modeChanged();
		void	stateChanged();
		void	remainingSecondsChanged();
		void	totalSecondsChanged();
		void	displayTimeChanged();
		void	progressChanged();
		void	completedRoundsChanged();

		void	focusMinutesChanged();
		void	shortBreakMinutesChanged();
		void	longBreakMinutesChanged();
		void	roundsBeforeLongBreakChanged();
		void	autoStartBreaksChanged();
		void	autoStartFocusChanged();

		// Emitted the moment a session runs out. Phase 3 hangs the alarm off this,
		// phase 4 the task counter and phase 5 the session log.
		// Carries the finished session's length because by the time this fires the timer
		// has already been re-armed for the next one, so totalSeconds is no longer it.
		//
		// Qualified on purpose: moc records the parameter type verbatim, and QML cannot
		// resolve a bare "Mode" back to the registered enum.
		void	sessionFinished(PomodoroTimer::Mode finished, PomodoroTimer::Mode next, int durationSeconds);

	private slots:
		void	onTick();

	private:
		static constexpr int	TickIntervalMs = 250;

		QTimer			_tickTimer;
		QElapsedTimer	_elapsed;

		Mode	_mode = Focus;
		State	_state = Idle;

		int		_focusMinutes = 25;
		int		_shortBreakMinutes = 5;
		int		_longBreakMinutes = 15;
		int		_roundsBeforeLongBreak = 4;

		bool	_autoStartBreaks = false;
		bool	_autoStartFocus = false;

		int		_completedRounds = 0;

		// The session clock is kept in milliseconds and derived from _elapsed rather
		// than counted down one tick at a time, so a long session cannot accumulate
		// the drift a plain "remaining -= 1" timer would.
		qint64	_totalMs = 0;
		qint64	_consumedMs = 0;
		qint64	_remainingMs = 0;
		int		_remainingSeconds = 0;

		int		minutesFor(Mode mode) const;
		Mode	nextMode() const;

		void	setState(State state);
		void	applyMode(Mode mode);
		void	refresh();
		void	finishSession();
};

#endif
