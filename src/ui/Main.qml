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

	PomodoroTimer
	{
		id: pomodoroTimer
	}

	TopMenu
	{
		themeColor: mainWindow.themeColor
		progress: pomodoroTimer.progress
	}

	TimerDisplay
	{
		anchors.centerIn: parent

		width: mainWindow.width * 0.5
		height: mainWindow.height * 0.5

		timer: pomodoroTimer
		themeColor: mainWindow.themeColor
	}

}
