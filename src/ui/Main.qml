import QtQuick
import QtQuick.Controls

import Pomodoro

Window
{
	id: mainWindow
    width: 1920 * 0.5
    height: 1080 * 0.5
    visible: true
    title: "pomodoro"

	// The window manager enforces these during a resize, so the drag handles simply stop
	// rather than letting the layout collapse. Kept deliberately wider than they are
	// tall: this layout is designed landscape, and at its smallest it should still look
	// like it. 640 x 420 is the tightest shape the title bar and mode tabs still fit in.
	minimumWidth: 640
	minimumHeight: 420

	flags: Qt.Window | Qt.FramelessWindowHint

	// Closing puts the app in the system tray rather than ending it, so a running session
	// survives pressing close out of habit. Clicking the tray icon brings it back.
	//
	// Because this swallows the close, quitting has to be reachable some other way: the
	// tray menu, Ctrl+Q, and the Quit button at the foot of the settings drawer.
	onClosing: (event) =>
	{
		if (!AppSettings.closeMinimizes)
		{
			Qt.quit()
			return
		}

		event.accepted = false

		if (TrayIcon.available)
			mainWindow.hide()
		else
		{
			// No tray to hide into. Minimising at least leaves the window reachable
			// from the task switcher instead of vanishing with no way back.
			mainWindow.showMinimized()
		}
	}

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

		onSessionFinished: (finished, next, durationSeconds) =>
		{
			SoundPlayer.playAlarm()

			// A finished focus session counts towards whatever task was being worked on,
			// and the history remembers which one it was.
			let taskId = finished === PomodoroTimer.Focus && AppSettings.tasksEnabled
				? taskList.creditFocusSession()
				: ""

			sessionLog.recordSession(finished, durationSeconds, taskId)

			// totalSeconds is the next session's length by now, which is exactly what
			// the message wants to talk about.
			let minutes = Math.round(pomodoroTimer.totalSeconds / 60)

			if (finished === PomodoroTimer.Focus)
			{
				let rounds = pomodoroTimer.completedRounds

				Notifier.notify(
					"Focus session complete",
					next === PomodoroTimer.LongBreak
						? rounds + " rounds done. Take a longer " + minutes + " minute break."
						: "That makes " + rounds + ". Take a " + minutes + " minute break.")
			}
			else
			{
				Notifier.notify(
					finished === PomodoroTimer.LongBreak ? "Long break over" : "Break over",
					"Back to focus for " + minutes + " minutes.")
			}
		}
	}

	SessionLog
	{
		id: sessionLog
	}

	TaskList
	{
		id: taskList
	}

	// Also forces the singleton into existence at start-up, so the samples are decoded
	// well before the first session ends instead of on the first play.
	Binding
	{
		target: SoundPlayer
		property: "volume"
		value: AppSettings.alarmVolume
	}

	// Whichever source is picked in the settings, as the one string MusicPlayer takes: a
	// stream URL, a YouTube link, or "spotify:" plus the URI to start ("spotify:" alone
	// resumes whatever Spotify last played).
	readonly property string musicSource:
	{
		switch (AppSettings.musicSource)
		{
			case "youtube":
				return AppSettings.youtubeUrl
			case "spotify":
				return AppSettings.spotifyUri.length > 0 ? AppSettings.spotifyUri : "spotify:"
			case "custom":
				return AppSettings.streamUrl
			default:
				return AppSettings.radioUrl
		}
	}

	Binding
	{
		target: MusicPlayer
		property: "source"
		value: mainWindow.musicSource
	}

	// The user's own Client ID wins over the one the build carries.
	Binding
	{
		target: MusicPlayer.spotify
		property: "clientId"
		value: AppSettings.spotifyClientId.length > 0
			? AppSettings.spotifyClientId
			: MusicPlayer.spotify.builtInClientId
	}

	Binding
	{
		target: MusicPlayer
		property: "volume"
		value: AppSettings.musicVolume
	}

	// With "play only during focus" on, the stream follows the timer instead of the
	// button. Off, the button is the only thing that decides.
	readonly property bool musicShouldFollow: AppSettings.musicFollowsFocus
		&& pomodoroTimer.state === PomodoroTimer.Running
		&& pomodoroTimer.mode === PomodoroTimer.Focus

	onMusicShouldFollowChanged:
	{
		if (!AppSettings.musicFollowsFocus)
			return

		if (mainWindow.musicShouldFollow)
			MusicPlayer.play()
		else
			MusicPlayer.pause()
	}

	TopMenu
	{
		id: topMenu

		anchors.top: parent.top

		themeColor: mainWindow.themeColor

		// Only one drawer at a time.
		onSettingsRequested:
			mainWindow.toggleDrawer(settingsPanel)

		onStatsRequested:
			mainWindow.toggleDrawer(statsPanel)

		onTasksRequested:
			mainWindow.toggleDrawer(tasksPanel)

		onMusicToggled:
			MusicPlayer.toggle()
	}

	// Lives here rather than inside TopMenu, which used to anchor it past its own
	// bottom edge to get it into position. Now it is simply the second thing in the
	// column, which is what lets the mode tabs anchor to it.
	Rectangle
	{
		id: progressTrack

		anchors.top: topMenu.bottom
		anchors.topMargin: 40
		anchors.horizontalCenter: parent.horizontalCenter

		width: parent.width * 0.95
		height: 8
		radius: 4

		color: Qt.rgba(1, 1, 1, 0.15)

		Rectangle
		{
			height: parent.height
			radius: parent.radius

			color: "white"
			opacity: 0.9

			// Deliberately not animated. The timer republishes its progress four times a
			// second and the bar creeps by well under a pixel each time, so a Behavior
			// here spent the whole session interpolating a change nobody can see -- and
			// because each animation lasted exactly until the next update arrived, the
			// scene never went still and Qt Quick redrew at the full refresh rate for
			// twenty-five minutes at a stretch. Stepping straight to the new width looks
			// identical and lets the window sit idle between updates.
			width: parent.width * pomodoroTimer.progress
		}
	}

	// Pinned under the progress bar rather than hung off the timer panel. Hanging it
	// off the panel meant it floated with the panel's centring, and on a short window
	// it drifted up into the bar.
	ModeTabs
	{
		id: modeTabs

		anchors.top: progressTrack.bottom
		anchors.topMargin: 22
		anchors.horizontalCenter: parent.horizontalCenter

		timer: pomodoroTimer
	}

	TimerDisplay
	{
		id: timerDisplay

		anchors.horizontalCenter: parent.horizontalCenter

		// Centred as before, but never allowed above the mode tabs. On a tall window
		// the first term wins and nothing changes; on a short one the floor takes over
		// and the panel is pushed down instead of climbing into the tabs.
		y: Math.max(modeTabs.y + modeTabs.height + 15,
			(mainWindow.height - timerDisplay.height) / 2 + 24)

		width: mainWindow.width * 0.5

		// Half the window height, but never taller than 16:9 of its own width. Without
		// the cap the panel's shape just mirrors the window's, so squaring off the
		// window squared off the panel, which is what made shrinking look wrong. At the
		// default size the two are identical, so nothing changes until it gets narrow.
		height: Math.min(mainWindow.height * 0.5, timerDisplay.width * 0.5625)

		timer: pomodoroTimer
		themeColor: mainWindow.themeColor

		// The same preference the settings drawer edits, so the two can never disagree.
		onMinutesRequested: (minutes) =>
		{
			if (pomodoroTimer.mode === PomodoroTimer.Focus)
				AppSettings.focusMinutes = minutes
			else if (pomodoroTimer.mode === PomodoroTimer.ShortBreak)
				AppSettings.shortBreakMinutes = minutes
			else
				AppSettings.longBreakMinutes = minutes
		}
	}

	// What is being worked on, under the timer. Clicking it opens the list, and with no
	// task picked it offers to pick one, so the feature can be found from the home screen.
	Text
	{
		id: activeTask

		anchors.top: timerDisplay.bottom
		anchors.topMargin: 14
		anchors.horizontalCenter: parent.horizontalCenter

		width: Math.min(implicitWidth, timerDisplay.width)

		visible: AppSettings.tasksEnabled

		text: taskList.activeTitle.length > 0
			? taskList.activeTitle + "  ·  " + taskList.activeCompleted + "/" + taskList.activeEstimate
			: taskList.openCount > 0 ? "Pick a task to work on" : "+ Add a task"

		textFormat: Text.PlainText
		color: "white"
		opacity: activeTaskHover.hovered ? 1.0 : (taskList.activeTitle.length > 0 ? 0.9 : 0.6)
		font.pixelSize: 16
		font.bold: taskList.activeTitle.length > 0
		elide: Text.ElideRight
		horizontalAlignment: Text.AlignHCenter

		HoverHandler
		{
			id: activeTaskHover
			cursorShape: Qt.PointingHandCursor
		}

		TapHandler
		{
			onTapped:
				mainWindow.toggleDrawer(tasksPanel)
		}
	}

	// Last, so the drawers and their scrims sit above everything else.
	SettingsPanel
	{
		id: settingsPanel

		anchors.fill: parent
		themeColor: mainWindow.themeColor
	}

	StatsPanel
	{
		id: statsPanel

		anchors.fill: parent
		themeColor: mainWindow.themeColor

		log: sessionLog
	}

	TasksPanel
	{
		id: tasksPanel

		anchors.fill: parent
		themeColor: mainWindow.themeColor

		tasks: taskList
	}

	// Turning the list off while its drawer is open would leave a drawer with no button.
	Connections
	{
		target: AppSettings

		function onTasksEnabledChanged()
		{
			if (!AppSettings.tasksEnabled)
				tasksPanel.open = false
		}
	}

	function toggleDrawer(drawer)
	{
		let wanted = !drawer.open

		settingsPanel.open = false
		statsPanel.open = false
		tasksPanel.open = false

		drawer.open = wanted
	}

	// Space, R and S are single letters, so they have to stay out of the way of any
	// text field that currently has the keyboard.
	readonly property bool typing: settingsPanel.typing || timerDisplay.typing || tasksPanel.typing

	Shortcut
	{
		sequences: ["Space"]
		enabled: !mainWindow.typing

		onActivated:
		{
			SoundPlayer.playClick()
			pomodoroTimer.toggle()
		}
	}

	Shortcut
	{
		sequences: ["R"]
		enabled: !mainWindow.typing

		onActivated:
		{
			SoundPlayer.playClick()
			pomodoroTimer.reset()
		}
	}

	Shortcut
	{
		sequences: ["S"]
		enabled: !mainWindow.typing

		onActivated:
		{
			SoundPlayer.playClick()
			pomodoroTimer.skip()
		}
	}

	// Whether the window was maximised before it was last minimised, so coming back from
	// the mini timer or the tray returns it to the size it left at.
	property bool wasMaximized: false

	onVisibilityChanged:
	{
		if (mainWindow.visibility === Window.Maximized)
			mainWindow.wasMaximized = true
		else if (mainWindow.visibility === Window.Windowed)
			mainWindow.wasMaximized = false
	}

	function restoreWindow()
	{
		// show() alone leaves a minimised window minimised.
		if (mainWindow.wasMaximized)
			mainWindow.showMaximized()
		else
			mainWindow.showNormal()

		mainWindow.raise()
		mainWindow.requestActivate()
	}

	// Stands in for the window while it is minimised. Not while it is hidden in the tray:
	// that is the user putting the app away on purpose, and the tray icon already carries
	// the time in its tooltip.
	MiniTimer
	{
		id: miniTimer

		timer: pomodoroTimer
		themeColor: mainWindow.themeColor

		visible: AppSettings.miniTimer && mainWindow.visibility === Window.Minimized

		onRestoreRequested:
			mainWindow.restoreWindow()
	}

	// Instantiated rather than merely referenced, so the bus name is claimed at start-up
	// and a later copy of the app has something to call. This is also the way back in if
	// the desktop claimed to have a tray and then did not draw the icon: launching
	// pomodoro again always restores the window.
	Connections
	{
		target: InstanceBridge

		function onRaiseRequested()
		{
			mainWindow.restoreWindow()
		}
	}

	// The tray entry is shown for the whole session, not only while hidden, so its menu
	// is always reachable.
	Binding
	{
		target: TrayIcon
		property: "visible"
		value: TrayIcon.available
	}

	Binding
	{
		target: TrayIcon
		property: "windowVisible"
		value: mainWindow.visible
	}

	Binding
	{
		target: TrayIcon
		property: "timerRunning"
		value: pomodoroTimer.state === PomodoroTimer.Running
	}

	Binding
	{
		target: TrayIcon
		property: "tooltip"
		value: (pomodoroTimer.mode === PomodoroTimer.Focus
				? "Focus"
				: pomodoroTimer.mode === PomodoroTimer.ShortBreak ? "Short break" : "Long break")
			+ " — " + pomodoroTimer.displayTime
	}

	Connections
	{
		target: TrayIcon

		function onShowRequested()
		{
			mainWindow.restoreWindow()
		}

		function onHideRequested()
		{
			mainWindow.hide()
		}

		function onToggleTimerRequested()
		{
			pomodoroTimer.toggle()
		}

		function onSkipRequested()
		{
			pomodoroTimer.skip()
		}

		function onQuitRequested()
		{
			Qt.quit()
		}
	}

	// The way out, given the close button no longer is one.
	Shortcut
	{
		sequences: ["Ctrl+Q"]

		onActivated:
			Qt.quit()
	}

	Shortcut
	{
		sequences: ["Ctrl+,"]

		onActivated:
			mainWindow.toggleDrawer(settingsPanel)
	}

	Shortcut
	{
		sequences: ["Ctrl+T"]
		enabled: AppSettings.tasksEnabled

		onActivated:
			mainWindow.toggleDrawer(tasksPanel)
	}

	// Escape closes whichever drawer is open rather than doing nothing.
	Shortcut
	{
		sequences: ["Escape"]
		enabled: settingsPanel.open || statsPanel.open || tasksPanel.open

		onActivated:
		{
			settingsPanel.open = false
			statsPanel.open = false
			tasksPanel.open = false
		}
	}

	// Every launch asks GitHub once whether there is something newer, a few seconds in so
	// it never competes with the window coming up. Nothing is shown unless there is.
	Timer
	{
		interval: 4000
		running: true

		onTriggered:
			UpdateChecker.check()
	}

	// The offer to update, along the bottom of the home screen. "Later" puts it away for
	// this run only; the next launch asks again.
	Rectangle
	{
		id: updateBanner

		property bool dismissed: false

		readonly property bool wanted: UpdateChecker.updateAvailable && !updateBanner.dismissed

		anchors.horizontalCenter: parent.horizontalCenter
		anchors.bottom: parent.bottom
		anchors.bottomMargin: updateBanner.wanted ? 18 : -updateBanner.height - 10

		Behavior on anchors.bottomMargin
		{
			NumberAnimation { duration: 300; easing.type: Easing.OutCubic }
		}

		visible: updateBanner.wanted || updateBanner.anchors.bottomMargin > -updateBanner.height - 10

		width: Math.min(parent.width - 32, bannerRow.implicitWidth + 36)
		height: 48
		radius: 12

		color: Qt.darker(mainWindow.themeColor, 1.35)
		border.color: Qt.rgba(1, 1, 1, 0.28)
		border.width: 1

		Row
		{
			id: bannerRow

			anchors.centerIn: parent
			spacing: 14

			Text
			{
				anchors.verticalCenter: parent.verticalCenter

				// While downloading or installing, the status line is the news; before
				// that, just what is on offer.
				text: UpdateChecker.busy || UpdateChecker.status === UpdateChecker.Failed
					? UpdateChecker.statusText
					: "Pomodoro " + UpdateChecker.latestVersion + " is available"

				color: UpdateChecker.status === UpdateChecker.Failed ? "#ffb4b4" : "white"
				font.pixelSize: 14
				elide: Text.ElideRight
				width: Math.min(implicitWidth, mainWindow.width - 300)
			}

			BannerButton
			{
				visible: !UpdateChecker.busy
				primary: true
				label: UpdateChecker.status === UpdateChecker.Failed
					? "Try again"
					: UpdateChecker.canInstall ? "Update now" : "Download"

				onClicked:
					UpdateChecker.installUpdate()
			}

			BannerButton
			{
				visible: !UpdateChecker.busy
				label: "Later"

				onClicked:
					updateBanner.dismissed = true
			}
		}

		// The download's progress, along the banner's bottom edge.
		Rectangle
		{
			anchors.left: parent.left
			anchors.bottom: parent.bottom
			anchors.margins: 1

			visible: UpdateChecker.status === UpdateChecker.Downloading
			width: (parent.width - 2) * UpdateChecker.downloadProgress
			height: 3
			radius: 1.5
			color: "white"
		}
	}

	component BannerButton: Button
	{
		id: bannerBtn

		property string label: ""
		property bool primary: false

		anchors.verticalCenter: parent ? parent.verticalCenter : undefined

		height: 32
		leftPadding: 14
		rightPadding: 14

		background: Rectangle
		{
			radius: 8
			color: bannerBtn.primary
				? (bannerBtn.hovered ? "#ffffff" : Qt.rgba(1, 1, 1, 0.9))
				: (bannerBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.08))
		}

		contentItem: Text
		{
			text: bannerBtn.label
			color: bannerBtn.primary ? "#333333" : "white"
			font.pixelSize: 13
			font.bold: bannerBtn.primary
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
	}

	// Absolutely last, so the window can still be resized while a drawer is open.
	ResizeHandles {}

}
