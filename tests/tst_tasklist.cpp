#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

#include "TaskList.hpp"

// The task list's rules, and that it survives a restart. Runs against Qt's test-mode
// data directory, so it never reads or overwrites the real tasks.json.
class TestTaskList : public QObject
{
	Q_OBJECT

	private slots:
		void	initTestCase();
		void	init();
		void	addTrimsAndRejectsEmptyTitles();
		void	firstTaskBecomesActive();
		void	creditCountsTowardsTheActiveTask();
		void	finishingTheActiveTaskClearsIt();
		void	estimateIsClamped();
		void	clearDoneKeepsOpenTasks();
		void	survivesARestart();
		void	notesAndStepsSurviveARestart();
		void	stepsAreTrimmedAndCounted();

	private:
		static void	wipe();
};

void	TestTaskList::initTestCase()
{
	QStandardPaths::setTestModeEnabled(true);
}

void	TestTaskList::init()
{
	wipe();
}

void	TestTaskList::wipe()
{
	QFile::remove(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
		+ QStringLiteral("/tasks.json"));
}

void	TestTaskList::addTrimsAndRejectsEmptyTitles()
{
	TaskList	tasks;

	QVERIFY(!tasks.add(QStringLiteral("   ")));
	QCOMPARE(tasks.count(), 0);

	QVERIFY(tasks.add(QStringLiteral("  write   the report ")));
	QCOMPARE(tasks.count(), 1);
	QCOMPARE(tasks.data(tasks.index(0), TaskList::TitleRole).toString(), QStringLiteral("write the report"));
}

void	TestTaskList::firstTaskBecomesActive()
{
	TaskList	tasks;

	tasks.add(QStringLiteral("first"));
	tasks.add(QStringLiteral("second"));

	QCOMPARE(tasks.activeTitle(), QStringLiteral("first"));

	tasks.toggleActive(1);
	QCOMPARE(tasks.activeTitle(), QStringLiteral("second"));

	// Clicking the active task again puts it down.
	tasks.toggleActive(1);
	QVERIFY(tasks.activeTaskId().isEmpty());
}

void	TestTaskList::creditCountsTowardsTheActiveTask()
{
	TaskList	tasks;

	QVERIFY(tasks.creditFocusSession().isEmpty());

	tasks.add(QStringLiteral("focus on this"), 3);

	QString	id = tasks.creditFocusSession();

	QCOMPARE(id, tasks.activeTaskId());
	QCOMPARE(tasks.activeCompleted(), 1);
	QCOMPARE(tasks.activeEstimate(), 3);
}

void	TestTaskList::finishingTheActiveTaskClearsIt()
{
	TaskList	tasks;

	tasks.add(QStringLiteral("only task"));
	tasks.setDone(0, true);

	QVERIFY(tasks.activeTaskId().isEmpty());
	QCOMPARE(tasks.openCount(), 0);

	// Picking a finished task to work on reopens it.
	tasks.toggleActive(0);
	QCOMPARE(tasks.openCount(), 1);
	QCOMPARE(tasks.activeTitle(), QStringLiteral("only task"));
}

void	TestTaskList::estimateIsClamped()
{
	TaskList	tasks;

	tasks.add(QStringLiteral("task"), 0);
	QCOMPARE(tasks.data(tasks.index(0), TaskList::EstimateRole).toInt(), 1);

	tasks.setEstimate(0, 999);
	QCOMPARE(tasks.data(tasks.index(0), TaskList::EstimateRole).toInt(), int(TaskList::MaximumEstimate));
}

void	TestTaskList::clearDoneKeepsOpenTasks()
{
	TaskList	tasks;

	tasks.add(QStringLiteral("a"));
	tasks.add(QStringLiteral("b"));
	tasks.add(QStringLiteral("c"));

	tasks.setDone(0, true);
	tasks.setDone(2, true);
	tasks.clearDone();

	QCOMPARE(tasks.count(), 1);
	QCOMPARE(tasks.data(tasks.index(0), TaskList::TitleRole).toString(), QStringLiteral("b"));
}

void	TestTaskList::stepsAreTrimmedAndCounted()
{
	TaskList	tasks;

	tasks.add(QStringLiteral("report"));

	QVERIFY(!tasks.addStep(0, QStringLiteral("   ")));
	QVERIFY(!tasks.addStep(5, QStringLiteral("no such task")));
	QVERIFY(tasks.addStep(0, QStringLiteral("  outline  the   sections ")));
	QVERIFY(tasks.addStep(0, QStringLiteral("write the intro")));
	QVERIFY(tasks.addStep(0, QStringLiteral("proofread")));

	QVariantList	steps = tasks.data(tasks.index(0), TaskList::StepsRole).toList();

	QCOMPARE(steps.size(), 3);
	QCOMPARE(steps.at(0).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("outline the sections"));

	tasks.setStepDone(0, 1, true);
	QCOMPARE(tasks.activeStepCount(), 3);
	QCOMPARE(tasks.activeStepsDone(), 1);

	// Clearing a step's text keeps it; removing is its own action.
	tasks.renameStep(0, 2, QStringLiteral(""));
	QCOMPARE(tasks.data(tasks.index(0), TaskList::StepCountRole).toInt(), 3);

	tasks.removeStep(0, 0);
	QCOMPARE(tasks.data(tasks.index(0), TaskList::StepCountRole).toInt(), 2);
	QCOMPARE(tasks.data(tasks.index(0), TaskList::StepsDoneRole).toInt(), 1);
}

void	TestTaskList::notesAndStepsSurviveARestart()
{
	{
		TaskList	tasks;

		tasks.add(QStringLiteral("with details"));
		tasks.setNotes(0, QStringLiteral("  first line\n\nsecond line  "));
		tasks.addStep(0, QStringLiteral("one"));
		tasks.addStep(0, QStringLiteral("two"));
		tasks.setStepDone(0, 1, true);

		tasks.add(QStringLiteral("plain"));
	}

	TaskList	reloaded;

	QCOMPARE(reloaded.count(), 2);

	// The description keeps its lines; only the space around it goes.
	QCOMPARE(reloaded.data(reloaded.index(0), TaskList::NotesRole).toString(),
		QStringLiteral("first line\n\nsecond line"));

	QVariantList	steps = reloaded.data(reloaded.index(0), TaskList::StepsRole).toList();

	QCOMPARE(steps.size(), 2);
	QCOMPARE(steps.at(1).toMap().value(QStringLiteral("text")).toString(), QStringLiteral("two"));
	QVERIFY(steps.at(1).toMap().value(QStringLiteral("done")).toBool());

	QVERIFY(reloaded.data(reloaded.index(1), TaskList::NotesRole).toString().isEmpty());
	QCOMPARE(reloaded.data(reloaded.index(1), TaskList::StepCountRole).toInt(), 0);
}

void	TestTaskList::survivesARestart()
{
	QString	activeId;

	{
		TaskList	tasks;

		tasks.add(QStringLiteral("kept"), 4);
		tasks.creditFocusSession();
		activeId = tasks.activeTaskId();

		// Destroyed with a save still pending; the destructor has to flush it.
	}

	TaskList	reloaded;

	QCOMPARE(reloaded.count(), 1);
	QCOMPARE(reloaded.activeTaskId(), activeId);
	QCOMPARE(reloaded.activeCompleted(), 1);
	QCOMPARE(reloaded.activeEstimate(), 4);
}

QTEST_GUILESS_MAIN(TestTaskList)

#include "tst_tasklist.moc"
