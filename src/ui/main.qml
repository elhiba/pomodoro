import QtQuick
import QtQuick.Controls

Window {
	id: mainWindow
    width: 1920/2
    height: 1080/2
    visible: true
    title: "pomodoro"
    color: "#12130F"

	flags: Qt.Window | Qt.FramelessWindowHint

	TopMenu {}

}
