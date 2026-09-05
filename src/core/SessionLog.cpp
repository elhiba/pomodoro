#include "SessionLog.hpp"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QStandardPaths>
#include <QVariantMap>

namespace
{
	const char *const	FileName = "sessions.json";
	constexpr int		FormatVersion = 1;

	// One file for everything rather than one per month. At a realistic ten sessions a
	// day this is well under a megabyte a year, and keeping it whole means the lifetime
	// total and a streak of any length are answerable without hunting across files.
}

SessionLog::SessionLog(QObject *parent)
	: QObject(parent)
{
	_saveTimer.setSingleShot(true);
	_saveTimer.setInterval(SaveDelayMs);

	connect(&_saveTimer, &QTimer::timeout, this, &SessionLog::save);

	load();
}

SessionLog::~SessionLog()
{
	if (_saveTimer.isActive())
	{
		_saveTimer.stop();
		save();
	}
}

int	SessionLog::todayFocusMinutes() const
{
	return _byDay.value(QDate::currentDate()).focusSeconds / 60;
}

int	SessionLog::todayPomodoros() const
{
	return _byDay.value(QDate::currentDate()).pomodoros;
}

int	SessionLog::weekFocusMinutes() const
{
	QDate	today = QDate::currentDate();
	int		seconds = 0;

	for (int offset = 0; offset < RecentDayCount; offset++)
		seconds += _byDay.value(today.addDays(-offset)).focusSeconds;

	return seconds / 60;
}

int	SessionLog::totalPomodoros() const
{
	int	total = 0;

	for (const DayTally &tally : _byDay)
		total += tally.pomodoros;

	return total;
}

int	SessionLog::currentStreak() const
{
	QDate	cursor = QDate::currentDate();

	// A day that has not been worked yet must not break the streak, so start counting
	// from yesterday when today is still empty.
	if (_byDay.value(cursor).pomodoros == 0)
		cursor = cursor.addDays(-1);

	int	streak = 0;

	while (_byDay.value(cursor).pomodoros > 0)
	{
		streak++;
		cursor = cursor.addDays(-1);
	}

	return streak;
}

QVariantList	SessionLog::recentDays() const
{
	QVariantList	days;
	QDate			today = QDate::currentDate();
	QLocale			locale;

	for (int offset = RecentDayCount - 1; offset >= 0; offset--)
	{
		QDate		date = today.addDays(-offset);
		DayTally	tally = _byDay.value(date);

		QVariantMap	entry;

		entry.insert(QStringLiteral("label"), locale.dayName(date.dayOfWeek(), QLocale::ShortFormat));
		entry.insert(QStringLiteral("minutes"), tally.focusSeconds / 60);
		entry.insert(QStringLiteral("pomodoros"), tally.pomodoros);
		entry.insert(QStringLiteral("isToday"), offset == 0);

		days.append(entry);
	}

	return days;
}

int	SessionLog::recentPeakMinutes() const
{
	QDate	today = QDate::currentDate();
	int		peak = 0;

	for (int offset = 0; offset < RecentDayCount; offset++)
		peak = qMax(peak, _byDay.value(today.addDays(-offset)).focusSeconds / 60);

	return peak;
}

void	SessionLog::recordSession(PomodoroTimer::Mode mode, int durationSeconds)
{
	if (durationSeconds <= 0)
		return;

	Record	record;

	record.finishedAt = QDateTime::currentDateTime();
	record.mode = mode;
	record.durationSeconds = durationSeconds;

	_records.append(record);

	// Cheaper than rebuilding every tally for one new row.
	if (mode == PomodoroTimer::Focus)
	{
		DayTally	&tally = _byDay[record.finishedAt.date()];

		tally.focusSeconds += durationSeconds;
		tally.pomodoros++;
	}

	touch();
}

void	SessionLog::clearHistory()
{
	if (_records.isEmpty())
		return;

	_records.clear();
	_byDay.clear();

	touch();
}

QString	SessionLog::modeToString(PomodoroTimer::Mode mode)
{
	switch (mode)
	{
		case PomodoroTimer::ShortBreak:
			return QStringLiteral("shortBreak");
		case PomodoroTimer::LongBreak:
			return QStringLiteral("longBreak");
		case PomodoroTimer::Focus:
		default:
			return QStringLiteral("focus");
	}
}

PomodoroTimer::Mode	SessionLog::modeFromString(const QString &name)
{
	if (name == QLatin1String("shortBreak"))
		return PomodoroTimer::ShortBreak;

	if (name == QLatin1String("longBreak"))
		return PomodoroTimer::LongBreak;

	return PomodoroTimer::Focus;
}

void	SessionLog::rebuildTallies()
{
	_byDay.clear();

	for (const Record &record : _records)
	{
		if (record.mode != PomodoroTimer::Focus)
			continue;

		DayTally	&tally = _byDay[record.finishedAt.date()];

		tally.focusSeconds += record.durationSeconds;
		tally.pomodoros++;
	}
}

void	SessionLog::touch()
{
	emit statsChanged();
	_saveTimer.start();
}

QString	SessionLog::storagePath() const
{
	QString	directory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

	return directory + QLatin1Char('/') + QLatin1String(FileName);
}

void	SessionLog::load()
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
		qWarning() << "pomodoro: session history is not valid JSON:" << error.errorString();
		return;
	}

	QJsonArray	array = document.object().value(QStringLiteral("sessions")).toArray();

	_records.clear();
	_records.reserve(array.size());

	for (const QJsonValue &value : array)
	{
		if (!value.isObject())
			continue;

		QJsonObject	object = value.toObject();
		Record		record;

		record.finishedAt = QDateTime::fromString(
			object.value(QStringLiteral("finishedAt")).toString(), Qt::ISODate);

		// A row with no usable timestamp cannot be placed on any day, so it is dropped
		// rather than silently landing on the epoch.
		if (!record.finishedAt.isValid())
			continue;

		record.mode = modeFromString(object.value(QStringLiteral("mode")).toString());
		record.durationSeconds = qMax(0, object.value(QStringLiteral("durationSeconds")).toInt());
		record.taskId = object.value(QStringLiteral("taskId")).toString();

		_records.append(record);
	}

	rebuildTallies();

	emit statsChanged();
}

void	SessionLog::save()
{
	QString	path = storagePath();

	if (!QDir().mkpath(QFileInfo(path).absolutePath()))
	{
		qWarning() << "pomodoro: could not create the data directory for" << path;
		return;
	}

	QJsonArray	array;

	for (const Record &record : _records)
	{
		QJsonObject	object;

		object.insert(QStringLiteral("finishedAt"), record.finishedAt.toString(Qt::ISODate));
		object.insert(QStringLiteral("mode"), modeToString(record.mode));
		object.insert(QStringLiteral("durationSeconds"), record.durationSeconds);

		// Left empty for now. The task list will fill it in when it returns, so the
		// format does not have to change then.
		object.insert(QStringLiteral("taskId"), record.taskId);

		array.append(object);
	}

	QJsonObject	root;

	root.insert(QStringLiteral("version"), FormatVersion);
	root.insert(QStringLiteral("sessions"), array);

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
