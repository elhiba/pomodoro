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
	signal tasksRequested()

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
        icon.color: "#e3e3e3"

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

	// What is on, tucked in beside the music button: the station on the top line, the
	// track underneath. While the stream is still connecting or coming back after a
	// drop it says so instead, so a silent player is never a mystery.
	//
	// Sized to whatever is left between the button and the centred title, and hidden
	// outright when there is nothing to say, so an idle player leaves no stub behind.
	Item
	{
		id: nowPlaying

		anchors.left: musicBtn.right
		anchors.leftMargin: 8
		anchors.verticalCenter: parent.verticalCenter

		width: Math.max(0, pomodoroText.x - nowPlaying.x - 12)
		height: parent.height

		visible: nowPlaying.width > 40
			&& (MusicPlayer.active || MusicPlayer.failed || MusicPlayer.title.length > 0)

		readonly property bool showsStatus:
			MusicPlayer.status === MusicPlayer.Connecting
			|| MusicPlayer.status === MusicPlayer.Reconnecting
			|| MusicPlayer.failed

		readonly property string topLine:
		{
			if (nowPlaying.showsStatus)
				return MusicPlayer.status === MusicPlayer.Reconnecting
					? "Reconnecting… (attempt " + MusicPlayer.retryAttempt + ")"
					: MusicPlayer.failed ? "Stream failed" : "Connecting…"

			if (MusicPlayer.stationName.length > 0)
				return MusicPlayer.stationName + (MusicPlayer.genre.length > 0 ? " · " + MusicPlayer.genre : "")

			return MusicPlayer.genre.length > 0 ? MusicPlayer.genre : "Lo-fi stream"
		}

		readonly property string bottomLine:
			nowPlaying.showsStatus && MusicPlayer.title.length === 0
				? ""
				: MusicPlayer.title

		Column
		{
			anchors.verticalCenter: parent.verticalCenter
			width: parent.width
			spacing: 1

			Text
			{
				width: parent.width

				text: nowPlaying.topLine

				// The station and title come off the network, so they are shown as plain
				// text rather than the default AutoText, which would try to render a
				// crafted title as rich text.
				textFormat: Text.PlainText

				color: MusicPlayer.failed ? "#ff8a8a" : Qt.rgba(1, 1, 1, 0.65)
				font.pixelSize: 10
				font.letterSpacing: 0.4
				elide: Text.ElideRight
				maximumLineCount: 1

				// The reconnect line pulses, so a stalled stream reads as still trying
				// rather than stuck.
				SequentialAnimation on opacity
				{
					running: MusicPlayer.status === MusicPlayer.Reconnecting
						|| MusicPlayer.status === MusicPlayer.Connecting
					loops: Animation.Infinite
					alwaysRunToEnd: true

					NumberAnimation { to: 0.35; duration: 700; easing.type: Easing.InOutSine }
					NumberAnimation { to: 1.0; duration: 700; easing.type: Easing.InOutSine }
				}
			}

			Text
			{
				width: parent.width

				text: nowPlaying.bottomLine
				textFormat: Text.PlainText
				visible: text.length > 0
				color: "white"
				font.pixelSize: 12
				font.bold: true
				elide: Text.ElideRight
				maximumLineCount: 1
			}
		}

		// The full text on hover, since the line is elided most of the time.
		HoverHandler
		{
			id: nowPlayingHover
		}

		ToolTip.visible: nowPlayingHover.hovered && (nowPlaying.bottomLine.length > 0 || nowPlaying.showsStatus)
		ToolTip.delay: 500
		ToolTip.text: nowPlaying.showsStatus
			? MusicPlayer.statusText
			: (MusicPlayer.stationName.length > 0 ? MusicPlayer.stationName + "\n" : "") + MusicPlayer.title
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

		// The task list's drawer comes in from this side, so its button lives here too.
		// Drawn like the stats button: a checklist of three dots and lines.
		Button
		{
			id: tasksBtn

			width: 40
			height: 40

			visible: AppSettings.tasksEnabled

			background: Rectangle { color: tasksBtn.hovered ? "#929494" : "transparent" }

			contentItem: Item
			{
				Column
				{
					anchors.centerIn: parent
					spacing: 4

					Repeater
					{
						model: 3

						Row
						{
							spacing: 3

							Rectangle
							{
								width: 3
								height: 3
								radius: 1.5
								color: "#e3e3e3"
								anchors.verticalCenter: parent.verticalCenter
							}

							Rectangle
							{
								width: 13
								height: 2
								radius: 1
								color: "#e3e3e3"
								anchors.verticalCenter: parent.verticalCenter
							}
						}
					}
				}
			}

			onClicked:
				rootMenu.tasksRequested()
		}

		Button
		{
			id:minBtn
            width: 40
            height: 40

            background: Rectangle { color: minBtn.hovered ? "#929494" : "transparent" }
            
            icon.source: "assets/icons/minimize.svg"
            icon.color: "#e3e3e3"
            
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
            icon.color: "#e3e3e3"
            
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
            icon.color: "#e3e3e3"

			// close() rather than Qt.quit(): it runs the window's closing handler, which
			// is where anything still buffered gets a chance to reach disk.
			onClicked:
				Window.window.close() 
        }
    }
}
