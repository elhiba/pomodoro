import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Rectangle {
	id: rootMenu
    width: parent.width
    height: 40
    color: "#12130F"

	FontLoader {
        id: textFont
        source: "qrc:/pomodoroFont"
    }

	MouseArea
	{
        anchors.fill: parent
        onPressed:
			Window.window.startSystemMove()
    }

	Text
	{
		id: pomodoroText
		text: "Pomodoro"
		color: "white"

		font.family: textFont.name
		font.pixelSize: 25

		anchors.horizontalCenter: parent.horizontalCenter
		anchors.verticalCenter: parent.verticalCenter
		anchors.verticalCenterOffset: 2

	}

	Row {
        anchors.right: parent.right
        height: parent.height

        Button {
			id:minBtn
            width: 40
            height: 40
            background: Rectangle { color: parent.hovered ? "#929494" : "transparent" }
            
            icon.source: "qrc:/minimize"
            icon.color: "white"
            
            onClicked:
                Window.window.showMinimized()
        }

        Button {
			id:resizeBtn
            width: 40
            height: 40
            background: Rectangle { color: parent.hovered ? "#929494" : "transparent" }
            
            icon.source: Window.window.visibility === Window.Maximized ? "qrc:/maximizeReverse" : "qrc:/maximize"
            icon.color: "white"
            
            onClicked:
                if (Window.window.visibility === Window.Maximized)
                    Window.window.showNormal()
                else
                    Window.window.showMaximized()
        }

        Button {
			id: closeBtn
            width: 40
            height: 40
            
			background: Rectangle { color: parent.hovered ? "#d91629" : "transparent" }
            
            icon.source: "qrc:/close"
            icon.color: parent.hovered ? "black" : "white"

			contentItem: Item {
				Image {
				    id: closeIcon
				    source: "qrc:/close"
				    sourceSize: Qt.size(16, 16)
				    anchors.centerIn: parent
				    visible: false
				}
				MultiEffect {
				    source: closeIcon
				    anchors.fill: closeIcon
				    colorization: 1.0
				
				    colorizationColor: closeBtn.hovered ? "white" : "#f0f0f0"
				}
			}

            onClicked:
                Qt.quit() 
        }
    }
}
