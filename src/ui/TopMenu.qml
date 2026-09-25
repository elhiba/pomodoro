import QtQuick
import QtQuick.Controls

Rectangle
{
	id: rootMenu
    width: parent.width
    height: 40

	property color themeColor: "#12130F"

	// Shown on the task button; Main.qml hands it in from the task list.
	property int openTasks: 0

	signal settingsRequested()
	signal statsRequested()
	signal tasksRequested()
	signal musicPanelRequested()

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

	// The one music button. It wears the logo of the source in use -- the station list,
	// YouTube or Spotify -- once music has been started, and
	// a plain note before that. Clicking it opens the music panel, which is where play,
	// pause, skip and the choice of source all live; one button instead of three keeps
	// the title bar quiet.
	Button
	{
		id: musicBtn

		width: 40
		height: 40

		anchors.left: statsBtn.right

		readonly property bool started: MusicPlayer.status !== MusicPlayer.Idle

		background: Rectangle
		{
			color: musicBtn.hovered ? "#929494" : "transparent"
		}

		contentItem: Item
		{
			Image
			{
				id: musicLogo

				anchors.centerIn: parent

				width: 22
				height: 22

				// Drawn as an Image rather than a button icon so the brand colours are not
				// tinted away.
				source:
				{
					if (!musicBtn.started)
						return "assets/icons/musicNote.svg"

					switch (AppSettings.musicSource)
					{
						case "youtube": return "assets/icons/youtube.svg"
						case "spotify": return "assets/icons/spotify.svg"
						default: return "assets/icons/radio.svg"
					}
				}

				sourceSize.width: 44
				sourceSize.height: 44
				fillMode: Image.PreserveAspectFit

				// Full strength while it plays, dimmed while paused, and breathing while it
				// connects, so the state reads without opening anything.
				opacity: MusicPlayer.active || !musicBtn.started ? 1.0 : 0.5

				SequentialAnimation on scale
				{
					running: MusicPlayer.status === MusicPlayer.Connecting
						|| MusicPlayer.status === MusicPlayer.Reconnecting
					loops: Animation.Infinite
					alwaysRunToEnd: true

					NumberAnimation { to: 0.8; duration: 600; easing.type: Easing.InOutSine }
					NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutSine }
				}
			}

			// A stream that will not start gets a red mark rather than a silent logo.
			Rectangle
			{
				anchors.right: musicLogo.right
				anchors.bottom: musicLogo.bottom
				anchors.rightMargin: -3
				anchors.bottomMargin: -3

				width: 9
				height: 9
				radius: 4.5

				visible: MusicPlayer.failed
				color: "#ff5c5c"
				border.color: "white"
				border.width: 1.5
			}
		}

		ToolTip.visible: musicBtn.hovered
		ToolTip.delay: 600
		ToolTip.text: "Music"

		onClicked:
			rootMenu.musicPanelRequested()
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
		anchors.leftMargin: 6
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
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.verticalCenter: parent.verticalCenter
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

		// The full text on hover, since the line is elided most of the time, and a click
		// opens the music panel.
		HoverHandler
		{
			id: nowPlayingHover
			cursorShape: Qt.PointingHandCursor
		}

		TapHandler
		{
			onTapped:
				rootMenu.musicPanelRequested()
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

		// The task list's drawer comes in from this side, so its button lives here too --
		// but set apart from minimise, maximise and close by a gap and a divider, and
		// drawn as a rounded button with the number of open tasks, so it reads as part of
		// the app rather than one more window control.
		Item
		{
			width: tasksBtn.visible ? tasksBtn.width + 21 : 0
			height: parent.height

			Button
			{
				id: tasksBtn

				anchors.left: parent.left
				anchors.verticalCenter: parent.verticalCenter

				width: 36
				height: 32

				visible: AppSettings.tasksEnabled

				background: Rectangle
				{
					radius: 8
					color: tasksBtn.hovered ? Qt.rgba(1, 1, 1, 0.22) : Qt.rgba(1, 1, 1, 0.1)
				}

				icon.source: "assets/icons/tasks.svg"
				icon.color: "#ffffff"
				icon.width: 20
				icon.height: 20

				ToolTip.visible: tasksBtn.hovered
				ToolTip.delay: 600
				ToolTip.text: "Tasks (Ctrl+T)"

				onClicked:
					rootMenu.tasksRequested()

				// How many tasks are still open, when there are any.
				Rectangle
				{
					anchors.right: parent.right
					anchors.top: parent.top
					anchors.rightMargin: -5
					anchors.topMargin: -4

					visible: rootMenu.openTasks > 0
					width: Math.max(16, countText.implicitWidth + 8)
					height: 16
					radius: 8
					color: "white"

					Text
					{
						id: countText

						anchors.centerIn: parent
						text: rootMenu.openTasks > 99 ? "99+" : rootMenu.openTasks
						color: rootMenu.themeColor
						font.pixelSize: 10
						font.bold: true
					}
				}
			}

			Rectangle
			{
				anchors.right: parent.right
				anchors.rightMargin: 10
				anchors.verticalCenter: parent.verticalCenter

				visible: tasksBtn.visible
				width: 1
				height: 20
				color: Qt.rgba(1, 1, 1, 0.3)
			}
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
