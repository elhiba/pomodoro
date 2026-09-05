import QtQuick
import QtQuick.Controls

Window {
	id: mainWindow
    width: 1920 * 0.5
    height: 1080 * 0.5
    visible: true
    title: "pomodoro"

	flags: Qt.Window | Qt.FramelessWindowHint

	property real timerProgress: 1.0
    property string currentMode: "focus"

    property color themeColor: {
        if (currentMode === "focus") return "#ba4949"      // Soft Red
        if (currentMode === "shortBreak") return "#38858a" // Soft Mint
        if (currentMode === "longBreak") return "#397097"  // Soft Blue
        return "#12130F" 
    }

	color: themeColor

	Behavior on color { 
        ColorAnimation { duration: 500; easing.type: Easing.InOutQuad } 
    }

	TopMenu {}

	Behavior on timerProgress
	{
            NumberAnimation { duration: 1000; easing.type: Easing.Linear }
	}
	TimerDisplay
	{
		anchors.centerIn: parent

	}

}
