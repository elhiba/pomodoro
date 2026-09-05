#ifndef SESSION_LOG_HPP
#define SESSION_LOG_HPP

#include <QDate>
#include <QDateTime>
#include <QMap>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVariantList>
#include <QVector>

#include <QtQml/qqmlregistration.h>

#include "PomodoroTimer.hpp"

// Every finished session, and the numbers derived from them. Breaks are recorded too
// so the history is complete, but only focus sessions count towards the figures shown:
// nobody wants credit for resting.
class SessionLog : public QObject
{
	Q_OBJECT
	QML_ELEMENT

	Q_PROPERTY(int todayFocusMinutes READ todayFocusMinutes NOTIFY statsChanged)
	Q_PROPERTY(int todayPomodoros READ todayPomodoros NOTIFY statsChanged)
	Q_PROPERTY(int weekFocusMinutes READ weekFocusMinutes NOTIFY statsChanged)
	Q_PROPERTY(int totalPomodoros READ totalPomodoros NOTIFY statsChanged)
	Q_PROPERTY(int currentStreak READ currentStreak NOTIFY statsChanged)

	// The last seven days, oldest first, ready for a Repeater.
	Q_PROPERTY(QVariantList recentDays READ recentDays NOTIFY statsChanged)
	Q_PROPERTY(int recentPeakMinutes READ recentPeakMinutes NOTIFY statsChanged)

	public:
		explicit SessionLog(QObject *parent = nullptr);
		~SessionLog() override;

		int		todayFocusMinutes() const;
		int		todayPomodoros() const;
		int		weekFocusMinutes() const;
		int		totalPomodoros() const;
		int		currentStreak() const;

		QVariantList	recentDays() const;
		int				recentPeakMinutes() const;

	public slots:
		void	recordSession(PomodoroTimer::Mode mode, int durationSeconds);
		void	clearHistory();

	signals:
		void	statsChanged();

	private:
		static constexpr int	SaveDelayMs = 400;
		static constexpr int	RecentDayCount = 7;

		struct Record
		{
			QDateTime			finishedAt;
			PomodoroTimer::Mode	mode = PomodoroTimer::Focus;
			int					durationSeconds = 0;
			QString				taskId;
		};

		struct DayTally
		{
			int	focusSeconds = 0;
			int	pomodoros = 0;
		};

		QVector<Record>			_records;
		QMap<QDate, DayTally>	_byDay;

		QTimer	_saveTimer;

		static QString				modeToString(PomodoroTimer::Mode mode);
		static PomodoroTimer::Mode	modeFromString(const QString &name);

		void	rebuildTallies();
		void	touch();

		QString	storagePath() const;
		void	load();
		void	save();
};

#endif
