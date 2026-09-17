#include <QSignalSpy>
#include <QTest>

#include "PomodoroTimer.hpp"

// Covers the session state machine, which is the one piece of the app that is pure
// logic and can run without a window, a bus or a sound card. Anything that needs a
// session to run all the way out is left alone: the shortest session is a minute and
// the clock is a real QElapsedTimer, so there is nothing to fast forward.
class TestPomodoroTimer : public QObject
{
	Q_OBJECT

	private slots:
		void	initialState();
		void	displayTimeIsZeroPadded();
		void	minutesAreClampedAndReappliedWhileIdle();
		void	runningSessionIsNotRelengthened();
		void	startPauseResumeReset();
		void	remainingCountsDownWhileRunning();
		void	skipCyclesThroughModesWithoutCountingRounds();
		void	setModePreservesSessionState();
		void	roundsBeforeLongBreakHasAFloor();
		void	automationFlagsRoundTrip();
};

void	TestPomodoroTimer::initialState()
{
	PomodoroTimer	timer;

	QCOMPARE(timer.mode(), PomodoroTimer::Focus);
	QCOMPARE(timer.state(), PomodoroTimer::Idle);
	QCOMPARE(timer.focusMinutes(), 25);
	QCOMPARE(timer.totalSeconds(), 25 * 60);
	QCOMPARE(timer.remainingSeconds(), 25 * 60);
	QCOMPARE(timer.displayTime(), QStringLiteral("25:00"));
	QCOMPARE(timer.completedRounds(), 0);
	QVERIFY(qFuzzyCompare(timer.progress(), 1.0));
}

void	TestPomodoroTimer::displayTimeIsZeroPadded()
{
	PomodoroTimer	timer;

	timer.setFocusMinutes(5);
	QCOMPARE(timer.displayTime(), QStringLiteral("05:00"));

	timer.setFocusMinutes(120);
	QCOMPARE(timer.displayTime(), QStringLiteral("120:00"));
}

void	TestPomodoroTimer::minutesAreClampedAndReappliedWhileIdle()
{
	PomodoroTimer	timer;
	QSignalSpy		totalChanged(&timer, &PomodoroTimer::totalSecondsChanged);

	// Zero and negatives collapse to the one minute floor.
	timer.setFocusMinutes(0);
	QCOMPARE(timer.focusMinutes(), 1);
	QCOMPARE(timer.totalSeconds(), 60);
	QCOMPARE(timer.remainingSeconds(), 60);
	QCOMPARE(totalChanged.count(), 1);

	timer.setFocusMinutes(-7);
	QCOMPARE(timer.focusMinutes(), 1);

	// A break length changes nothing while a focus session is armed...
	timer.setShortBreakMinutes(9);
	QCOMPARE(timer.totalSeconds(), 60);

	// ...until that break is the current mode.
	timer.setMode(PomodoroTimer::ShortBreak);
	QCOMPARE(timer.totalSeconds(), 9 * 60);
}

void	TestPomodoroTimer::runningSessionIsNotRelengthened()
{
	PomodoroTimer	timer;

	timer.setFocusMinutes(10);
	timer.start();

	// Only an idle session may be re-lengthened under the user.
	timer.setFocusMinutes(30);
	QCOMPARE(timer.focusMinutes(), 30);
	QCOMPARE(timer.totalSeconds(), 10 * 60);

	timer.reset();
	QCOMPARE(timer.totalSeconds(), 30 * 60);
}

void	TestPomodoroTimer::startPauseResumeReset()
{
	PomodoroTimer	timer;
	QSignalSpy		stateChanged(&timer, &PomodoroTimer::stateChanged);

	timer.start();
	QCOMPARE(timer.state(), PomodoroTimer::Running);

	// Starting twice is a no-op, not a restart.
	timer.start();
	QCOMPARE(stateChanged.count(), 1);

	timer.pause();
	QCOMPARE(timer.state(), PomodoroTimer::Paused);

	timer.toggle();
	QCOMPARE(timer.state(), PomodoroTimer::Running);

	timer.toggle();
	QCOMPARE(timer.state(), PomodoroTimer::Paused);

	timer.reset();
	QCOMPARE(timer.state(), PomodoroTimer::Idle);
	QCOMPARE(timer.remainingSeconds(), timer.totalSeconds());
	QVERIFY(qFuzzyCompare(timer.progress(), 1.0));
}

void	TestPomodoroTimer::remainingCountsDownWhileRunning()
{
	PomodoroTimer	timer;
	QSignalSpy		displayChanged(&timer, &PomodoroTimer::displayTimeChanged);

	timer.start();

	// Ticks are 250 ms apart and seconds round up, so after a little over a second
	// the display has moved exactly one second, whatever the scheduler did.
	QTRY_COMPARE_WITH_TIMEOUT(timer.remainingSeconds(), timer.totalSeconds() - 1, 3000);
	QVERIFY(displayChanged.count() >= 1);
	QVERIFY(timer.progress() < 1.0);

	timer.pause();

	int	frozen = timer.remainingSeconds();

	QTest::qWait(600);
	QCOMPARE(timer.remainingSeconds(), frozen);
}

void	TestPomodoroTimer::skipCyclesThroughModesWithoutCountingRounds()
{
	PomodoroTimer	timer;

	timer.skip();
	QCOMPARE(timer.mode(), PomodoroTimer::ShortBreak);
	QCOMPARE(timer.state(), PomodoroTimer::Idle);
	QCOMPARE(timer.totalSeconds(), timer.shortBreakMinutes() * 60);

	timer.skip();
	QCOMPARE(timer.mode(), PomodoroTimer::Focus);

	// A skipped focus session was not completed, so it never earns a long break.
	for (int round = 0; round < 2 * timer.roundsBeforeLongBreak(); round++)
		timer.skip();

	QCOMPARE(timer.completedRounds(), 0);
	QVERIFY(timer.mode() != PomodoroTimer::LongBreak);
}

void	TestPomodoroTimer::setModePreservesSessionState()
{
	PomodoroTimer	timer;
	QSignalSpy		modeChanged(&timer, &PomodoroTimer::modeChanged);

	timer.start();

	timer.setMode(PomodoroTimer::Focus);
	QCOMPARE(timer.state(), PomodoroTimer::Running);
	QCOMPARE(modeChanged.count(), 0);

	timer.setMode(PomodoroTimer::LongBreak);
	QCOMPARE(timer.mode(), PomodoroTimer::LongBreak);
	QCOMPARE(timer.state(), PomodoroTimer::Idle);
	QCOMPARE(timer.remainingSeconds(), timer.longBreakMinutes() * 60);
	QCOMPARE(modeChanged.count(), 1);

	timer.setMode(PomodoroTimer::Focus);
	QCOMPARE(timer.mode(), PomodoroTimer::Focus);
	QCOMPARE(timer.state(), PomodoroTimer::Running);
	QCOMPARE(modeChanged.count(), 2);
}

void	TestPomodoroTimer::roundsBeforeLongBreakHasAFloor()
{
	PomodoroTimer	timer;

	timer.setRoundsBeforeLongBreak(0);
	QCOMPARE(timer.roundsBeforeLongBreak(), 1);

	timer.setRoundsBeforeLongBreak(6);
	QCOMPARE(timer.roundsBeforeLongBreak(), 6);
}

void	TestPomodoroTimer::automationFlagsRoundTrip()
{
	PomodoroTimer	timer;
	QSignalSpy		breaksChanged(&timer, &PomodoroTimer::autoStartBreaksChanged);

	QVERIFY(!timer.autoStartBreaks());
	QVERIFY(!timer.autoStartFocus());

	timer.setAutoStartBreaks(true);
	timer.setAutoStartBreaks(true);
	QVERIFY(timer.autoStartBreaks());
	QCOMPARE(breaksChanged.count(), 1);

	timer.setAutoStartFocus(true);
	QVERIFY(timer.autoStartFocus());
}

QTEST_GUILESS_MAIN(TestPomodoroTimer)

#include "tst_pomodorotimer.moc"
