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
