import QtQuick
import QtQuick.Controls

Rectangle
{
	id: rootMenu
    width: parent.width
    height: 40

	property color themeColor: "#12130F"

	signal settingsRequested()
	signal statsRequested()
	signal musicToggled()

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
        //
        // The Row goes inside an Item so it can be centred. A Row used directly as a
        // contentItem gets stretched to the whole content rect and lays its children
        // out from its own left edge, which left the bars hard against the left side.
        contentItem: Item
        {
            Row
            {
                anchors.centerIn: parent

                spacing: 3

                // Anchored to a shared baseline rather than positioned by hand, so the
                // bars line up at the bottom whatever their heights are.
                Rectangle
                {
                    width: 3
                    height: 8
                    radius: 1
                    color: "#e3e3e3"
                    anchors.bottom: parent.bottom
                }

                Rectangle
                {
                    width: 3
                    height: 16
                    radius: 1
                    color: "#e3e3e3"
                    anchors.bottom: parent.bottom
                }

                Rectangle
                {
                    width: 3
                    height: 12
                    radius: 1
                    color: "#e3e3e3"
                    anchors.bottom: parent.bottom
                }
            }
        }

        onClicked:
            rootMenu.statsRequested()
    }

	Button
	{
		id: musicBtn

        width: 40
        height: 40

        anchors.left: statsBtn.right

        background: Rectangle
        {
            color: musicBtn.hovered ? "#929494" : "transparent"
        }

        icon.source: MusicPlayer.active
            ? "assets/icons/pauseTimer.svg"
            : "assets/icons/playTimer.svg"

        // Red while the stream is refusing to start, and faded while it is still trying,
        // so a dead URL is visible without opening the settings.
        icon.color: MusicPlayer.failed ? "#ff8a8a" : "#e3e3e3"

        opacity: MusicPlayer.status === MusicPlayer.Connecting ? 0.55 : 1.0

        Behavior on opacity
        {
            NumberAnimation { duration: 200 }
        }

        onClicked:
            MusicPlayer.toggle()
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

			// close() rather than Qt.quit(): it runs the window's closing handler, which
			// is where anything still buffered gets a chance to reach disk.
			onClicked:
				Window.window.close() 
        }
    }
}
