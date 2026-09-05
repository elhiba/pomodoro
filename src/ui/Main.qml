import QtQuick

import Pomodoro

Window
{
	id: mainWindow
    width: 1920 * 0.5
    height: 1080 * 0.5
    visible: true
    title: "pomodoro"

	flags: Qt.Window | Qt.FramelessWindowHint

    property color themeColor:
	{
        if (pomodoroTimer.mode === PomodoroTimer.Focus) return "#ba4949"      // Soft Red
        if (pomodoroTimer.mode === PomodoroTimer.ShortBreak) return "#38858a" // Soft Mint
        if (pomodoroTimer.mode === PomodoroTimer.LongBreak) return "#397097"  // Soft Blue
        return "#12130F" 
    }

	color: themeColor

	Behavior on color
	{ 
        ColorAnimation { duration: 500; easing.type: Easing.InOutQuad } 
    }

	// The stored preferences feed the timer one way. The timer never writes back,
	// so the settings panel stays the only thing that can change them.
	PomodoroTimer
	{
		id: pomodoroTimer

		focusMinutes: AppSettings.focusMinutes
		shortBreakMinutes: AppSettings.shortBreakMinutes
		longBreakMinutes: AppSettings.longBreakMinutes
		roundsBeforeLongBreak: AppSettings.roundsBeforeLongBreak

		autoStartBreaks: AppSettings.autoStartBreaks
		autoStartFocus: AppSettings.autoStartFocus

		onSessionFinished:
			SoundPlayer.playAlarm()
	}

	// Also forces the singleton into existence at start-up, so the samples are decoded
	// well before the first session ends instead of on the first play.
	Binding
	{
		target: SoundPlayer
		property: "volume"
		value: AppSettings.alarmVolume
	}

	TopMenu
	{
		themeColor: mainWindow.themeColor
		progress: pomodoroTimer.progress

		onSettingsRequested:
			settingsPanel.open = !settingsPanel.open
	}

	ModeTabs
	{
		anchors.bottom: timerDisplay.top
		anchors.bottomMargin: 20
		anchors.horizontalCenter: parent.horizontalCenter

		timer: pomodoroTimer
	}

	TimerDisplay
	{
		id: timerDisplay

		anchors.centerIn: parent
		anchors.verticalCenterOffset: 24

		width: mainWindow.width * 0.5
		height: mainWindow.height * 0.5

		timer: pomodoroTimer
		themeColor: mainWindow.themeColor
	}

	// Last, so the drawer and its scrim sit above everything else.
	SettingsPanel
	{
		id: settingsPanel

		anchors.fill: parent
		themeColor: mainWindow.themeColor
	}

}
