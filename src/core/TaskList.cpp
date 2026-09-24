#include "TaskList.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

namespace
{
	const char *const	FileName = "tasks.json";
	constexpr int		FormatVersion = 1;
}

TaskList::TaskList(QObject *parent)
	: QAbstractListModel(parent)
{
	_saveTimer.setSingleShot(true);
	_saveTimer.setInterval(SaveDelayMs);

	connect(&_saveTimer, &QTimer::timeout, this, &TaskList::save);

	load();
}

TaskList::~TaskList()
{
	if (_saveTimer.isActive())
	{
		_saveTimer.stop();
		save();
	}
}

int	TaskList::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : static_cast<int>(_tasks.size());
}

QVariant	TaskList::data(const QModelIndex &index, int role) const
{
	if (!index.isValid() || !validRow(index.row()))
		return QVariant();

	const Task	&task = _tasks.at(index.row());

	switch (role)
	{
		case TaskIdRole:
			return task.id;
		case Qt::DisplayRole:
		case TitleRole:
			return task.title;
		case DoneRole:
			return task.done;
		case EstimateRole:
			return task.estimate;
		case CompletedRole:
			return task.completed;
		case ActiveRole:
			return task.id == _activeId;
		default:
			return QVariant();
	}
}

QHash<int, QByteArray>	TaskList::roleNames() const
{
	return {
		{ TaskIdRole, "taskId" },
		{ TitleRole, "title" },
		{ DoneRole, "done" },
		{ EstimateRole, "estimate" },
		{ CompletedRole, "completed" },
		{ ActiveRole, "active" }
	};
}

int	TaskList::count() const
{
	return static_cast<int>(_tasks.size());
}

int	TaskList::openCount() const
{
	int	open = 0;

	for (const Task &task : _tasks)
	{
		if (!task.done)
			open++;
	}

	return open;
}

QString	TaskList::activeTaskId() const
{
	return _activeId;
}

QString	TaskList::activeTitle() const
{
	int	row = rowOf(_activeId);

	return row < 0 ? QString() : _tasks.at(row).title;
}

int	TaskList::activeCompleted() const
{
	int	row = rowOf(_activeId);

	return row < 0 ? 0 : _tasks.at(row).completed;
}

int	TaskList::activeEstimate() const
{
	int	row = rowOf(_activeId);

	return row < 0 ? 0 : _tasks.at(row).estimate;
}

bool	TaskList::add(const QString &title, int estimate)
{
	QString	trimmed = title.simplified().left(MaximumTitleLength);

	if (trimmed.isEmpty())
		return false;

	Task	task;

	task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	task.title = trimmed;
	task.estimate = qBound(1, estimate, MaximumEstimate);

	int	row = static_cast<int>(_tasks.size());

	beginInsertRows(QModelIndex(), row, row);
	_tasks.append(task);
	endInsertRows();

	emit countChanged();

	// The first task of an empty list is almost certainly the one about to be worked on,
	// so it is picked for the user rather than making them click it as well.
	if (_activeId.isEmpty())
		setActiveId(task.id);

	touch();
	return true;
}

void	TaskList::remove(int row)
{
	if (!validRow(row))
		return;

	bool	wasActive = _tasks.at(row).id == _activeId;

	beginRemoveRows(QModelIndex(), row, row);
	_tasks.removeAt(row);
	endRemoveRows();

	emit countChanged();

	if (wasActive)
		setActiveId(QString());

	touch();
}

void	TaskList::rename(int row, const QString &title)
{
	if (!validRow(row))
		return;

	QString	trimmed = title.simplified().left(MaximumTitleLength);

	// Clearing a title is not a way to delete a task; the old one stays.
	if (trimmed.isEmpty() || trimmed == _tasks.at(row).title)
		return;

	_tasks[row].title = trimmed;
	changed(row, { TitleRole });

	if (_tasks.at(row).id == _activeId)
		emit activeChanged();

	touch();
}

void	TaskList::setDone(int row, bool done)
{
	if (!validRow(row) || _tasks.at(row).done == done)
		return;

	_tasks[row].done = done;
	changed(row, { DoneRole });

	emit countChanged();

	// A finished task stops collecting sessions.
	if (done && _tasks.at(row).id == _activeId)
		setActiveId(QString());

	touch();
}

void	TaskList::setEstimate(int row, int estimate)
{
	estimate = qBound(1, estimate, MaximumEstimate);

	if (!validRow(row) || _tasks.at(row).estimate == estimate)
		return;

	_tasks[row].estimate = estimate;
	changed(row, { EstimateRole });

	if (_tasks.at(row).id == _activeId)
		emit activeChanged();

	touch();
}

void	TaskList::toggleActive(int row)
{
	if (!validRow(row))
		return;

	const Task	&task = _tasks.at(row);

	if (task.id == _activeId)
	{
		setActiveId(QString());
		return;
	}

	// Picking a finished task to work on reopens it.
	if (task.done)
		setDone(row, false);

	setActiveId(_tasks.at(row).id);
}

void	TaskList::clearDone()
{
	bool	removed = false;

	for (int row = static_cast<int>(_tasks.size()) - 1; row >= 0; row--)
	{
		if (!_tasks.at(row).done)
			continue;

		beginRemoveRows(QModelIndex(), row, row);
		_tasks.removeAt(row);
		endRemoveRows();

		removed = true;
	}

	if (!removed)
		return;

	emit countChanged();
	touch();
}

QString	TaskList::creditFocusSession()
{
	int	row = rowOf(_activeId);

	if (row < 0)
		return QString();

	_tasks[row].completed++;
	changed(row, { CompletedRole });

	emit activeChanged();
	touch();

	return _activeId;
}

int	TaskList::rowOf(const QString &id) const
{
	if (id.isEmpty())
		return -1;

	for (int row = 0; row < _tasks.size(); row++)
	{
		if (_tasks.at(row).id == id)
			return row;
	}

	return -1;
}

bool	TaskList::validRow(int row) const
{
	return row >= 0 && row < _tasks.size();
}

void	TaskList::changed(int row, const QList<int> &roles)
{
	QModelIndex	at = index(row);

	emit dataChanged(at, at, roles);
}

void	TaskList::setActiveId(const QString &id)
{
	if (_activeId == id)
		return;

	int	previous = rowOf(_activeId);

	_activeId = id;

	if (previous >= 0)
		changed(previous, { ActiveRole });

	int	current = rowOf(_activeId);

	if (current >= 0)
		changed(current, { ActiveRole });

	emit activeChanged();
	touch();
}

void	TaskList::touch()
{
	_saveTimer.start();
}

QString	TaskList::storagePath() const
{
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

	return directory + QLatin1Char('/') + QLatin1String(FileName);
}

void	TaskList::load()
{
	QFile	file(storagePath());

	if (!file.exists())
		return;

	if (!file.open(QIODevice::ReadOnly))
	{
		qWarning() << "pomodoro: could not read" << file.fileName() << file.errorString();
		return;
	}

	QJsonParseError	error;
	QJsonDocument	document = QJsonDocument::fromJson(file.readAll(), &error);

	if (error.error != QJsonParseError::NoError || !document.isObject())
	{
		qWarning() << "pomodoro: task list is not valid JSON:" << error.errorString();
		return;
	}

	QJsonObject	root = document.object();
	QJsonArray	array = root.value(QStringLiteral("tasks")).toArray();

	for (const QJsonValue &value : array)
	{
		if (!value.isObject())
			continue;

		QJsonObject	object = value.toObject();
		Task		task;

		task.id = object.value(QStringLiteral("id")).toString();
		task.title = object.value(QStringLiteral("title")).toString().simplified().left(MaximumTitleLength);

		// A row without an id or a title is not something the user could have made.
		if (task.id.isEmpty() || task.title.isEmpty() || rowOf(task.id) >= 0)
			continue;

		task.done = object.value(QStringLiteral("done")).toBool();
		task.estimate = qBound(1, object.value(QStringLiteral("estimate")).toInt(1), MaximumEstimate);
		task.completed = qMax(0, object.value(QStringLiteral("completed")).toInt());

		_tasks.append(task);
	}

	QString	active = root.value(QStringLiteral("activeTaskId")).toString();

	if (rowOf(active) >= 0)
		_activeId = active;
}

void	TaskList::save()
{
	QString	path = storagePath();

	if (!QDir().mkpath(QFileInfo(path).absolutePath()))
	{
		qWarning() << "pomodoro: could not create the data directory for" << path;
		return;
	}

	QJsonArray	array;

	for (const Task &task : _tasks)
	{
		QJsonObject	object;

		object.insert(QStringLiteral("id"), task.id);
		object.insert(QStringLiteral("title"), task.title);
		object.insert(QStringLiteral("done"), task.done);
		object.insert(QStringLiteral("estimate"), task.estimate);
		object.insert(QStringLiteral("completed"), task.completed);

		array.append(object);
	}

	QJsonObject	root;

	root.insert(QStringLiteral("version"), FormatVersion);
	root.insert(QStringLiteral("activeTaskId"), _activeId);
	root.insert(QStringLiteral("tasks"), array);

	QSaveFile	file(path);

	if (!file.open(QIODevice::WriteOnly))
	{
		qWarning() << "pomodoro: could not write" << path << file.errorString();
		return;
	}

	file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));

	if (!file.commit())
		qWarning() << "pomodoro: could not commit" << path << file.errorString();
}
