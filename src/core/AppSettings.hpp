#ifndef APP_SETTINGS_HPP
#define APP_SETTINGS_HPP

#include <QObject>
#include <QSettings>
#include <QString>
#include <QVariant>

#include <QtQml/qqmlregistration.h>

// Everything the app remembers between runs. Registered as a QML singleton, so any
// component can read it without it having to be handed down the tree, and every setter
// writes straight through to QSettings instead of waiting for a save step.
//
// Named AppSettings rather than Settings on purpose: QtCore already exports a QML type
// called Settings, and the two would collide.
class AppSettings : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_SINGLETON

	Q_PROPERTY(int focusMinutes READ focusMinutes WRITE setFocusMinutes NOTIFY focusMinutesChanged)
	Q_PROPERTY(int shortBreakMinutes READ shortBreakMinutes WRITE setShortBreakMinutes NOTIFY shortBreakMinutesChanged)
	Q_PROPERTY(int longBreakMinutes READ longBreakMinutes WRITE setLongBreakMinutes NOTIFY longBreakMinutesChanged)
	Q_PROPERTY(int roundsBeforeLongBreak READ roundsBeforeLongBreak WRITE setRoundsBeforeLongBreak NOTIFY roundsBeforeLongBreakChanged)

	Q_PROPERTY(bool autoStartBreaks READ autoStartBreaks WRITE setAutoStartBreaks NOTIFY autoStartBreaksChanged)
	Q_PROPERTY(bool autoStartFocus READ autoStartFocus WRITE setAutoStartFocus NOTIFY autoStartFocusChanged)

	Q_PROPERTY(qreal alarmVolume READ alarmVolume WRITE setAlarmVolume NOTIFY alarmVolumeChanged)
	Q_PROPERTY(bool alwaysOnTop READ alwaysOnTop WRITE setAlwaysOnTop NOTIFY alwaysOnTopChanged)
	Q_PROPERTY(bool minimizeToTray READ minimizeToTray WRITE setMinimizeToTray NOTIFY minimizeToTrayChanged)

	// Handy for the settings panel, so the bounds live in one place instead of
	// being repeated in QML.
	Q_PROPERTY(int minimumMinutes READ minimumMinutes CONSTANT)
	Q_PROPERTY(int maximumMinutes READ maximumMinutes CONSTANT)
	Q_PROPERTY(int minimumRounds READ minimumRounds CONSTANT)
	Q_PROPERTY(int maximumRounds READ maximumRounds CONSTANT)

	public:
		static constexpr int	DefaultFocusMinutes = 25;
		static constexpr int	DefaultShortBreakMinutes = 5;
		static constexpr int	DefaultLongBreakMinutes = 15;
		static constexpr int	DefaultRoundsBeforeLongBreak = 4;
		static constexpr bool	DefaultAutoStartBreaks = false;
		static constexpr bool	DefaultAutoStartFocus = false;
		static constexpr qreal	DefaultAlarmVolume = 0.7;
		static constexpr bool	DefaultAlwaysOnTop = false;
		static constexpr bool	DefaultMinimizeToTray = false;

		static constexpr int	MinimumMinutes = 1;
		static constexpr int	MaximumMinutes = 120;
		static constexpr int	MinimumRounds = 1;
		static constexpr int	MaximumRounds = 12;

		explicit AppSettings(QObject *parent = nullptr);
		~AppSettings() override;

		int		focusMinutes() const;
		int		shortBreakMinutes() const;
		int		longBreakMinutes() const;
		int		roundsBeforeLongBreak() const;
		bool	autoStartBreaks() const;
		bool	autoStartFocus() const;
		qreal	alarmVolume() const;
		bool	alwaysOnTop() const;
		bool	minimizeToTray() const;

		int		minimumMinutes() const;
		int		maximumMinutes() const;
		int		minimumRounds() const;
		int		maximumRounds() const;

		void	setFocusMinutes(int minutes);
		void	setShortBreakMinutes(int minutes);
		void	setLongBreakMinutes(int minutes);
		void	setRoundsBeforeLongBreak(int rounds);
		void	setAutoStartBreaks(bool autoStart);
		void	setAutoStartFocus(bool autoStart);
		void	setAlarmVolume(qreal volume);
		void	setAlwaysOnTop(bool onTop);
		void	setMinimizeToTray(bool toTray);

	public slots:
		void	restoreDefaults();

	signals:
		void	focusMinutesChanged();
		void	shortBreakMinutesChanged();
		void	longBreakMinutesChanged();
		void	roundsBeforeLongBreakChanged();
		void	autoStartBreaksChanged();
		void	autoStartFocusChanged();
		void	alarmVolumeChanged();
		void	alwaysOnTopChanged();
		void	minimizeToTrayChanged();

	private:
		QSettings	_store;

		int		_focusMinutes = DefaultFocusMinutes;
		int		_shortBreakMinutes = DefaultShortBreakMinutes;
		int		_longBreakMinutes = DefaultLongBreakMinutes;
		int		_roundsBeforeLongBreak = DefaultRoundsBeforeLongBreak;
		bool	_autoStartBreaks = DefaultAutoStartBreaks;
		bool	_autoStartFocus = DefaultAutoStartFocus;
		qreal	_alarmVolume = DefaultAlarmVolume;
		bool	_alwaysOnTop = DefaultAlwaysOnTop;
		bool	_minimizeToTray = DefaultMinimizeToTray;

		void	load();
		void	store(const char *key, const QVariant &value);
};

#endif
