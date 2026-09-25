#ifndef TASK_LIST_HPP
#define TASK_LIST_HPP

#include <QAbstractListModel>
#include <QString>
#include <QTimer>
#include <QVector>

#include <QtQml/qqmlregistration.h>

// The to-do list: what the focus sessions are for. A plain list model so QML can put it
// straight into a ListView, saved to tasks.json next to the session history.
//
// Each task can carry a description and a checklist of steps: what exactly is left to do
// for it. Both are optional, and a task without them is saved exactly as before.
//
// One task at a time can be the active one. A focus session that finishes while a task is
// active counts towards it, and its id is what SessionLog records against the session.
// Nothing here knows about the timer; Main.qml calls creditFocusSession() when one ends.
class TaskList : public QAbstractListModel
{
	Q_OBJECT
	QML_ELEMENT

	Q_PROPERTY(int count READ count NOTIFY countChanged)
	Q_PROPERTY(int openCount READ openCount NOTIFY countChanged)
	Q_PROPERTY(QString activeTaskId READ activeTaskId NOTIFY activeChanged)
	Q_PROPERTY(QString activeTitle READ activeTitle NOTIFY activeChanged)
	Q_PROPERTY(int activeCompleted READ activeCompleted NOTIFY activeChanged)
	Q_PROPERTY(int activeEstimate READ activeEstimate NOTIFY activeChanged)
	Q_PROPERTY(int activeStepCount READ activeStepCount NOTIFY activeChanged)
	Q_PROPERTY(int activeStepsDone READ activeStepsDone NOTIFY activeChanged)

	public:
		enum Role
		{
			TaskIdRole = Qt::UserRole + 1,
			TitleRole,
			DoneRole,
			EstimateRole,
			CompletedRole,
			ActiveRole,
			NotesRole,
			StepsRole,		// [{ text, done }, ...] in order
			StepCountRole,
			StepsDoneRole
		};
		Q_ENUM(Role)

		static constexpr int	MaximumEstimate = 20;
		static constexpr int	MaximumTitleLength = 200;
		static constexpr int	MaximumNotesLength = 4000;
		static constexpr int	MaximumStepLength = 200;
		static constexpr int	MaximumSteps = 100;

		explicit TaskList(QObject *parent = nullptr);
		~TaskList() override;

		int			rowCount(const QModelIndex &parent = QModelIndex()) const override;
		QVariant	data(const QModelIndex &index, int role = Qt::DisplayRole) const override;

		QHash<int, QByteArray>	roleNames() const override;

		int		count() const;
		int		openCount() const;
		QString	activeTaskId() const;
		QString	activeTitle() const;
		int		activeCompleted() const;
		int		activeEstimate() const;
		int		activeStepCount() const;
		int		activeStepsDone() const;

	public slots:
		// Returns false for a title that is empty once trimmed.
		bool	add(const QString &title, int estimate = 1);
		void	remove(int row);
		void	rename(int row, const QString &title);
		void	setDone(int row, bool done);
		void	setEstimate(int row, int estimate);

		// The task's description. Line breaks are kept; surrounding blank space is not.
		void	setNotes(int row, const QString &notes);

		// The checklist. addStep() returns false for a step that is empty once trimmed,
		// or when the task already has MaximumSteps.
		bool	addStep(int row, const QString &text);
		void	renameStep(int row, int step, const QString &text);
		void	setStepDone(int row, int step, bool done);
		void	removeStep(int row, int step);

		// Makes the row the active task, or clears it if it already was.
		void	toggleActive(int row);
		void	clearDone();

		// Counts a finished focus session towards the active task and returns its id, or
		// an empty string when no task is active.
		QString	creditFocusSession();

	signals:
		void	countChanged();
		void	activeChanged();

	private:
		static constexpr int	SaveDelayMs = 400;

		struct Step
		{
			QString	text;
			bool	done = false;
		};

		struct Task
		{
			QString			id;
			QString			title;
			QString			notes;
			QVector<Step>	steps;
			bool			done = false;
			int				estimate = 1;
			int				completed = 0;

			int	stepsDone() const;
		};

		QVector<Task>	_tasks;
		QString			_activeId;
		QTimer			_saveTimer;

		int		rowOf(const QString &id) const;
		bool	validRow(int row) const;
		bool	validStep(int row, int step) const;
		void	stepsChanged(int row);
		static QVariantList	stepList(const Task &task);
		static QString		cleanNotes(const QString &notes);
		void	changed(int row, const QList<int> &roles);
		void	setActiveId(const QString &id);
		void	touch();

		QString	storagePath() const;
		void	load();
		void	save();
};

#endif
