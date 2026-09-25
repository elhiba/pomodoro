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
			// The music steps aside for the length of the alarm (about two seconds) and a
			// little after, so the alarm is never drowned out.
			MusicPlayer.duck(3000)
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
		openTasks: taskList.openCount

		// Only one drawer at a time.
		onSettingsRequested:
			mainWindow.toggleDrawer(settingsPanel)

		onStatsRequested:
			mainWindow.toggleDrawer(statsPanel)

		onTasksRequested:
			mainWindow.toggleDrawer(tasksPanel)

		onMusicPanelRequested:
			mainWindow.toggleDrawer(musicPanel)
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

	MusicPanel
	{
		id: musicPanel

		anchors.fill: parent
		themeColor: mainWindow.themeColor
		stations: mainWindow.stations

		onSkipRequested: (step) => mainWindow.skipMusic(step)
		onStationAdded: (name, note, url) => mainWindow.addStation(name, note, url)
		onStationRemoved: (url) => mainWindow.removeStation(url)

		spotifyShelf: mainWindow.spotifyShelf
		onSpotifyPicked: (uri) => mainWindow.playSpotify(uri)
		onSpotifyAdded: (name, uri, image) => mainWindow.addSpotifyLink(name, uri, image)
		onSpotifyRemoved: (uri) => mainWindow.removeSpotifyLink(uri)
	}

	// What the built-in Spotify player offers without search (Spotify refuses lookups
	// from it): focus playlists by Spotify and Lofi Girl, each checked through oEmbed and
	// played through the player on 2026-09-25.
	readonly property var spotifyPicks: [
		{ name: "lofi beats", uri: "spotify:playlist:37i9dQZF1DWWQRwui0ExPn", image: "https://i.scdn.co/image/ab67706f00000002266beb50b0032b0f140a749e" },
		{ name: "Lofi Girl - beats to relax/study to", uri: "spotify:playlist:0vvXsWCC9xrXsKd4FyS8kM", image: "https://image-cdn-ak.spotifycdn.com/image/ab67706c0000da848bc80c95b9d248cf462c0bd1" },
		{ name: "Deep Focus", uri: "spotify:playlist:37i9dQZF1DWZeKCadgRdKQ", image: "https://i.scdn.co/image/ab67706f000000026020f2f6476db518ef747da4" },
		{ name: "Peaceful Piano", uri: "spotify:playlist:37i9dQZF1DX4sWSpwq3LiO", image: "https://i.scdn.co/image/ab67706f0000000270e1fb7db7b45809d6a80377" },
		{ name: "Jazz in the Background", uri: "spotify:playlist:37i9dQZF1DWV7EzJMK2FUI", image: "https://i.scdn.co/image/ab67706f00000002472120b92edea982b5feb264" },
		{ name: "Brain Food", uri: "spotify:playlist:37i9dQZF1DWXLeA8Omikj7", image: "https://i.scdn.co/image/ab67706f00000002e9f13d8a7595736a0a8efa72" },
		{ name: "Instrumental Study", uri: "spotify:playlist:37i9dQZF1DX9sIqqvKsjG8", image: "https://i.scdn.co/image/ab67706f000000026cb530ab9aafafdaab62f4bb" },
		{ name: "Coding Mode", uri: "spotify:playlist:37i9dQZF1DX5trt9i14X7j", image: "https://i.scdn.co/image/ab67706f00000002863b311d4b787ed621f7e696" },
		{ name: "Nature Sounds", uri: "spotify:playlist:37i9dQZF1DX4PP3DA4J0N8", image: "https://i.scdn.co/image/ab67706f00000002e909a20522cfdb017c798ba8" },
		{ name: "Ambient Relaxation", uri: "spotify:playlist:37i9dQZF1DX3Ogo9pFvBkY", image: "https://i.scdn.co/image/ab67706f0000000226b7b546f240016f02447692" }
	]

	// Links the user saved, stored as JSON like the stations.
	readonly property var savedSpotify:
	{
		try
		{
			let saved = JSON.parse(AppSettings.spotifySaved || "[]")

			return Array.isArray(saved)
				? saved.filter((item) => item && typeof item.uri === "string")
				: []
		}
		catch (error)
		{
			return []
		}
	}

	// Liked Songs first (once the player knows whose they are), then the picks, then the
	// user's own.
	readonly property var spotifyShelf:
	{
		let shelf = []

		if (MusicPlayer.spotify.likedSongsUri.length > 0)
			shelf.push({ name: "Liked Songs", uri: MusicPlayer.spotify.likedSongsUri, image: "" })

		return shelf.concat(mainWindow.spotifyPicks, mainWindow.savedSpotify.map((item) => ({
			name: item.name, uri: item.uri, image: item.image || "", custom: true
		})))
	}

	function playSpotify(uri)
	{
		AppSettings.spotifyUri = uri

		if (!MusicPlayer.active)
			MusicPlayer.play()
	}

	function addSpotifyLink(name, uri, image)
	{
		let saved = mainWindow.savedSpotify.filter((item) => item.uri !== uri)

		saved.push({ name: name, uri: uri, image: image })
		AppSettings.spotifySaved = JSON.stringify(saved)

		mainWindow.playSpotify(uri)
	}

	function removeSpotifyLink(uri)
	{
		AppSettings.spotifySaved = JSON.stringify(mainWindow.savedSpotify.filter((item) => item.uri !== uri))
	}

	// The radio list the app ships with. Every one was checked to answer with audio and an
	// icy-name before it went in; the notes say what to expect.
	readonly property var builtInStations: [
		{ name: "Lofi Music", note: "Lo-fi beats · Zeno", url: "https://stream.zeno.fm/f3wvbbqmdg8uv" },
		{ name: "Lofi Hip Hop Radio", note: "Lo-fi hip hop · Zeno", url: "https://stream.zeno.fm/0r0xa792kwzuv" },
		{ name: "Lofi", note: "Lo-fi · laut.fm", url: "https://stream.laut.fm/lofi" },
		{ name: "ChillHop", note: "Chillhop · FluxFM", url: "https://streams.fluxfm.de/Chillhop/mp3-128/streams.fluxfm.de/" },
		{ name: "Hunter.FM Lo-Fi", note: "Lo-fi · Hunter.FM", url: "https://live.hunter.fm/lofi_high" },
		{ name: "Fluid", note: "Instrumental hip hop · SomaFM", url: "https://ice1.somafm.com/fluid-128-mp3" },
		{ name: "Groove Salad", note: "Chilled ambient beats · SomaFM", url: "https://ice1.somafm.com/groovesalad-128-mp3" },
		{ name: "Deep Space One", note: "Deep ambient · SomaFM", url: "https://ice1.somafm.com/deepspaceone-128-mp3" },
		{ name: "Drone Zone", note: "Ambient, no beats · SomaFM", url: "https://ice1.somafm.com/dronezone-128-mp3" }
	]

	// The stations the user added, which only get in once StationProbe has heard audio
	// from them. Stored as JSON; anything unreadable counts as none rather than breaking
	// the list.
	readonly property var customStations:
	{
		try
		{
			let saved = JSON.parse(AppSettings.customStations || "[]")

			return Array.isArray(saved)
				? saved.filter((station) => station && typeof station.url === "string")
				: []
		}
		catch (error)
		{
			return []
		}
	}

	// Both, in that order: what the music panel shows and what skip walks through.
	readonly property var stations: mainWindow.builtInStations.concat(
		mainWindow.customStations.map((station) => ({
			name: station.name, note: station.note || "", url: station.url, custom: true
		})))

	// A checked station joins the end of the list and starts playing: whoever just added
	// it wants to hear it.
	function addStation(name, note, url)
	{
		let saved = mainWindow.customStations.filter((station) => station.url !== url)

		saved.push({ name: name, note: note, url: url })

		AppSettings.customStations = JSON.stringify(saved)
		AppSettings.radioUrl = url

		if (!MusicPlayer.active)
			MusicPlayer.play()
	}

	// Removing the station that is on moves to the first one, playing if it was.
	function removeStation(url)
	{
		AppSettings.customStations = JSON.stringify(
			mainWindow.customStations.filter((station) => station.url !== url))

		if (AppSettings.radioUrl === url)
			AppSettings.radioUrl = mainWindow.builtInStations[0].url
	}

	// Next and previous. On the radio list that is the next station, kept playing if it
	// was; everywhere else MusicPlayer knows what to do.
	function skipMusic(step)
	{
		if (AppSettings.musicSource !== "radio")
		{
			if (step > 0)
				MusicPlayer.next()
			else
				MusicPlayer.previous()

			return
		}

		let count = mainWindow.stations.length
		let current = 0

		for (let index = 0; index < count; index++)
		{
			if (mainWindow.stations[index].url === AppSettings.radioUrl)
				current = index
		}

		let wasPlaying = MusicPlayer.active

		AppSettings.radioUrl = mainWindow.stations[(current + step + count) % count].url

		if (!wasPlaying)
			MusicPlayer.play()
	}

	// A YouTube queue moving on is remembered as the YouTube choice.
	Connections
	{
		target: MusicPlayer

		function onYoutubeTrackChanged(url)
		{
			AppSettings.musicSource = "youtube"
			AppSettings.youtubeUrl = url
		}
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
		musicPanel.open = false

		drawer.open = wanted
	}

	// Space, R and S are single letters, so they have to stay out of the way of any
	// text field that currently has the keyboard.
	readonly property bool typing: timerDisplay.typing || tasksPanel.typing
		|| musicPanel.typing

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
		enabled: settingsPanel.open || statsPanel.open || tasksPanel.open || musicPanel.open

		onActivated:
		{
			settingsPanel.open = false
			statsPanel.open = false
			tasksPanel.open = false
			musicPanel.open = false
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
