import QtQuick
import QtQuick.Controls

Rectangle
{
	id: rootMenu
    width: parent.width
    height: 40

	property color themeColor: "#12130F"
	property real progress: 1.0

	signal settingsRequested()
	signal statsRequested()

    color: rootMenu.themeColor

	Behavior on color
	{ 
        ColorAnimation { duration: 500; easing.type: Easing.InOutQuad } 
    }

	FontLoader
	{
        id: textFont
        source: "assets/fonts/PlaywriteAR.ttf"
    }

	MouseArea
	{
        anchors.fill: parent
        onPressed:
			Window.window.startSystemMove()
    }

	Button
	{
		id: menuBtn

        width: 40
        height: 40

        anchors.left: parent.left

        background: Rectangle { color: menuBtn.hovered ? "#929494" : "transparent" }

        icon.source: "assets/icons/menu.svg"

        onClicked:
            rootMenu.settingsRequested()
    }

	Button
	{
		id: statsBtn

        width: 40
        height: 40

        anchors.left: menuBtn.right

        background: Rectangle { color: statsBtn.hovered ? "#929494" : "transparent" }

        // Drawn rather than loaded: there is no chart icon in assets, and a wrong
        // icon reads worse than three bars.
        contentItem: Row
        {
            spacing: 3

            Rectangle
            {
                width: 3
                height: 8
                y: (statsBtn.height - 16) / 2 + 8
                color: "#e3e3e3"
                radius: 1
            }

            Rectangle
            {
                width: 3
                height: 16
                y: (statsBtn.height - 16) / 2
                color: "#e3e3e3"
                radius: 1
            }

            Rectangle
            {
                width: 3
                height: 12
                y: (statsBtn.height - 16) / 2 + 4
                color: "#e3e3e3"
                radius: 1
            }
        }

        onClicked:
            rootMenu.statsRequested()
    }

	Text
	{
		id: pomodoroText
		text: "Pomodoro"
		color: "white"

		font.family: textFont.name
		font.pixelSize: 35

		anchors.horizontalCenter: parent.horizontalCenter
		anchors.verticalCenter: parent.verticalCenter
		anchors.verticalCenterOffset: 20

		MouseArea
		{
	        anchors.fill: parent
	        onPressed:
				Window.window.startSystemMove()
	    }
	}

	Row
	{
        anchors.right: parent.right
        height: parent.height

		Button
		{
			id:minBtn
            width: 40
            height: 40

            background: Rectangle { color: minBtn.hovered ? "#929494" : "transparent" }
            
            icon.source: "assets/icons/minimize.svg"
            
            onClicked:
                Window.window.showMinimized()
        }

		Button
		{
			id:resizeBtn
            width: 40
            height: 40

            background: Rectangle { color: resizeBtn.hovered ? "#929494" : "transparent" }
            
            icon.source: Window.window.visibility === Window.Maximized ? "assets/icons/maximizeReverse.svg" : "assets/icons/maximize.svg"
            
            onClicked:
                if (Window.window.visibility === Window.Maximized)
                    Window.window.showNormal()
                else
                    Window.window.showMaximized()
        }

		Button
		{
			id: closeBtn
            width: 40
            height: 40
            
			background: Rectangle { color: closeBtn.hovered ? "#d91629" : "transparent" }
            
            icon.source: "assets/icons/close.svg"

			onClicked:
				Qt.quit() 
        }
    }

	Rectangle
	{
        id: progressTrack
        height: 8
        radius: 4
        
        width: parent.width * 0.95
        anchors.horizontalCenter: parent.horizontalCenter
        
        anchors.top: parent.bottom
        anchors.topMargin: 40
        
        color: Qt.rgba(1, 1, 1, 0.15) 

        Rectangle {
            height: parent.height
            radius: parent.radius
            color: "white" 
            opacity: 0.9 
            
            width: parent.width * rootMenu.progress

            Behavior on width
            {
                NumberAnimation { duration: 250; easing.type: Easing.Linear }
            }
        }
    }
}
