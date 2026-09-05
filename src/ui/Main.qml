import QtQuick

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
			sessionLog.recordSession(finished, durationSeconds)

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

	// Also forces the singleton into existence at start-up, so the samples are decoded
	// well before the first session ends instead of on the first play.
	Binding
	{
		target: SoundPlayer
		property: "volume"
		value: AppSettings.alarmVolume
	}

	Binding
	{
		target: MusicPlayer
		property: "source"
		value: AppSettings.streamUrl
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

		// Only one drawer at a time, they come in from opposite sides.
		onSettingsRequested:
		{
			statsPanel.open = false
			settingsPanel.open = !settingsPanel.open
		}

		onStatsRequested:
		{
			settingsPanel.open = false
			statsPanel.open = !statsPanel.open
		}

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

			width: parent.width * pomodoroTimer.progress

			Behavior on width
			{
				NumberAnimation { duration: 250; easing.type: Easing.Linear }
			}
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

	// Space, R and S are single letters, so they have to stay out of the way of any
	// text field that currently has the keyboard.
	readonly property bool typing: settingsPanel.typing

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

	function restoreWindow()
	{
		mainWindow.show()
		mainWindow.raise()
		mainWindow.requestActivate()
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
		{
			statsPanel.open = false
			settingsPanel.open = !settingsPanel.open
		}
	}

	// Escape closes whichever drawer is open rather than doing nothing.
	Shortcut
	{
		sequences: ["Escape"]
		enabled: settingsPanel.open || statsPanel.open

		onActivated:
		{
			settingsPanel.open = false
			statsPanel.open = false
		}
	}

	// Absolutely last, so the window can still be resized while a drawer is open.
	ResizeHandles {}

}
