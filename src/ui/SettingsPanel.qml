import QtQuick
import QtQuick.Controls

import Pomodoro

// Slide in drawer from the right, plus the scrim behind it. Fills the whole window
// so it can dim everything underneath, but only the drawer itself is ever on screen
// when it is closed.
Item
{
	id: rootPanel

	property bool open: false
	property color themeColor: "#12130F"

	// Single letter shortcuts have to stand down while a URL is being typed.
	readonly property bool typing: streamField.activeFocus || youtubeField.activeFocus
		|| spotifyField.activeFocus || clientIdField.activeFocus

	// The radio list. Every one was checked to answer with audio and an icy-name before
	// it went in; the notes say what kind of music to expect.
	readonly property var stations: [
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

	// A text field for a link, committed on Enter or on losing focus rather than per
	// keystroke: a URL is invalid most of the way through being typed.
	component LinkField: TextField
	{
		id: linkField

		// Set by the owner when the committed text was not accepted.
		property bool rejected: false

		signal committed(string value)

		height: 38

		placeholderTextColor: Qt.rgba(1, 1, 1, 0.45)
		color: "white"
		font.pixelSize: 13
		clip: true

		background: Rectangle
		{
			radius: 8
			color: Qt.rgba(0, 0, 0, 0.2)
			border.color: linkField.rejected
				? "#ff8a8a"
				: linkField.activeFocus ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.22)
			border.width: 1

			Behavior on border.color
			{
				ColorAnimation { duration: 150 }
			}
		}

		onAccepted:
			linkField.committed(linkField.text.trim())

		onActiveFocusChanged:
			if (!linkField.activeFocus)
				linkField.committed(linkField.text.trim())
	}

	// The drawer's standard full-width button.
	component PanelButton: Button
	{
		id: panelBtn

		property string label: ""

		height: 38
		opacity: panelBtn.enabled ? 1.0 : 0.5

		background: Rectangle
		{
			radius: 8
			color: panelBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
			border.color: Qt.rgba(1, 1, 1, 0.25)
			border.width: 1
		}

		contentItem: Text
		{
			text: panelBtn.label
			color: "white"
			font.pixelSize: 14
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
			elide: Text.ElideRight
		}

		onClicked:
			SoundPlayer.playClick()
	}

	// Leaving the drawer armed would make the next visit one careless click from quitting.
	onOpenChanged:
		if (!rootPanel.open)
			quitBtn.armed = false

	Rectangle
	{
		id: scrim

		anchors.fill: parent
		color: "black"

		opacity: rootPanel.open ? 0.4 : 0.0
		visible: scrim.opacity > 0

		Behavior on opacity
		{
			NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
		}

		MouseArea
		{
			anchors.fill: parent

			onClicked:
				rootPanel.open = false
		}
	}

	Rectangle
	{
		id: drawer

		width: Math.min(380, rootPanel.width * 0.9)
		height: rootPanel.height

		x: rootPanel.open ? 0 : -width

		Behavior on x
		{
			NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
		}

		color: Qt.darker(rootPanel.themeColor, 1.4)

		Behavior on color
		{
			ColorAnimation { duration: 500; easing.type: Easing.InOutQuad }
		}

		// The drawer eats clicks so they do not reach the scrim underneath.
		MouseArea
		{
			anchors.fill: parent
		}

		Rectangle
		{
			id: header

			anchors.top: parent.top
			anchors.left: parent.left
			anchors.right: parent.right

			height: 56
			color: "transparent"

			Text
			{
				anchors.left: parent.left
				anchors.leftMargin: 20
				anchors.verticalCenter: parent.verticalCenter

				text: "Settings"
				color: "white"
				font.pixelSize: 20
				font.bold: true
			}

			Button
			{
				id: closePanelBtn

				width: 36
				height: 36

				anchors.right: parent.right
				anchors.rightMargin: 12
				anchors.verticalCenter: parent.verticalCenter

				background: Rectangle
				{
					radius: 6
					color: closePanelBtn.hovered ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
				}

				icon.source: "assets/icons/close.svg"
				icon.color: "white"
				icon.width: 16
				icon.height: 16

				onClicked:
					rootPanel.open = false
			}

			Rectangle
			{
				anchors.bottom: parent.bottom
				anchors.left: parent.left
				anchors.right: parent.right

				height: 1
				color: Qt.rgba(1, 1, 1, 0.15)
			}
		}

		ScrollView
		{
			id: body

			anchors.top: header.bottom
			anchors.bottom: parent.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.margins: 20

			clip: true
			contentWidth: availableWidth
			ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

			Column
			{
				width: body.availableWidth
				spacing: 4

				Text
				{
					text: "DURATIONS"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					bottomPadding: 6
				}

				NumberSetting
				{
					label: "Focus"
					value: AppSettings.focusMinutes
					minimum: AppSettings.minimumMinutes
					maximum: AppSettings.maximumMinutes

					onValueModified: (newValue) => AppSettings.focusMinutes = newValue
				}

				NumberSetting
				{
					label: "Short break"
					value: AppSettings.shortBreakMinutes
					minimum: AppSettings.minimumMinutes
					maximum: AppSettings.maximumMinutes

					onValueModified: (newValue) => AppSettings.shortBreakMinutes = newValue
				}

				NumberSetting
				{
					label: "Long break"
					value: AppSettings.longBreakMinutes
					minimum: AppSettings.minimumMinutes
					maximum: AppSettings.maximumMinutes

					onValueModified: (newValue) => AppSettings.longBreakMinutes = newValue
				}

				NumberSetting
				{
					label: "Rounds before long break"
					value: AppSettings.roundsBeforeLongBreak
					minimum: AppSettings.minimumRounds
					maximum: AppSettings.maximumRounds
					suffix: ""

					onValueModified: (newValue) => AppSettings.roundsBeforeLongBreak = newValue
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					text: "AUTOMATION"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 14
					bottomPadding: 6
				}

				ToggleSetting
				{
					label: "Start breaks automatically"
					checked: AppSettings.autoStartBreaks
					accentColor: rootPanel.themeColor

					onToggleRequested: (wanted) => AppSettings.autoStartBreaks = wanted
				}

				ToggleSetting
				{
					label: "Start focus automatically"
					checked: AppSettings.autoStartFocus
					accentColor: rootPanel.themeColor

					onToggleRequested: (wanted) => AppSettings.autoStartFocus = wanted
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					text: "TASKS"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 14
					bottomPadding: 6
				}

				ToggleSetting
				{
					label: "Show the task list"
					checked: AppSettings.tasksEnabled
					accentColor: rootPanel.themeColor

					onToggleRequested: (wanted) => AppSettings.tasksEnabled = wanted
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					text: "SOUND"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 14
					bottomPadding: 6
				}

				SliderSetting
				{
					label: "Alarm volume"
					value: AppSettings.alarmVolume

					onValueModified: (newValue) => AppSettings.alarmVolume = newValue
					onPreviewRequested: SoundPlayer.playAlarm()
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					text: "MUSIC"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 14
					bottomPadding: 6
				}

				// Where the music comes from. Each source keeps its own choice, so trying
				// another one and coming back finds things as they were left.
				Row
				{
					id: sourceTabs

					width: parent.width
					spacing: 4
					bottomPadding: 10

					Repeater
					{
						model: [
							{ key: "radio", label: "Radio" },
							{ key: "youtube", label: "YouTube" },
							{ key: "spotify", label: "Spotify" },
							{ key: "custom", label: "Link" }
						]

						delegate: Button
						{
							id: sourceTab

							required property var modelData

							readonly property bool current: AppSettings.musicSource === sourceTab.modelData.key

							width: (sourceTabs.width - 3 * sourceTabs.spacing) / 4
							height: 32

							background: Rectangle
							{
								radius: 8
								color: sourceTab.current
									? Qt.rgba(1, 1, 1, 0.28)
									: sourceTab.hovered ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.06)
							}

							contentItem: Text
							{
								text: sourceTab.modelData.label
								color: "white"
								font.pixelSize: 13
								font.bold: sourceTab.current
								horizontalAlignment: Text.AlignHCenter
								verticalAlignment: Text.AlignVCenter
							}

							onClicked:
								AppSettings.musicSource = sourceTab.modelData.key
						}
					}
				}

				// Radio: a short list of lo-fi and ambient stations that were checked to be
				// up and to carry now-playing titles.
				Column
				{
					width: parent.width
					spacing: 2
					visible: AppSettings.musicSource === "radio"

					Repeater
					{
						model: rootPanel.stations

						delegate: Rectangle
						{
							id: stationRow

							required property var modelData

							readonly property bool current: AppSettings.radioUrl === stationRow.modelData.url

							width: parent.width
							height: 44
							radius: 8

							color: stationRow.current
								? Qt.rgba(1, 1, 1, 0.2)
								: stationHover.hovered ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

							HoverHandler
							{
								id: stationHover
								cursorShape: Qt.PointingHandCursor
							}

							TapHandler
							{
								onTapped:
									AppSettings.radioUrl = stationRow.modelData.url
							}

							Rectangle
							{
								id: stationMark

								anchors.left: parent.left
								anchors.leftMargin: 10
								anchors.verticalCenter: parent.verticalCenter

								width: 14
								height: 14
								radius: 7

								color: "transparent"
								border.color: "white"
								border.width: 2

								Rectangle
								{
									anchors.centerIn: parent
									width: 6
									height: 6
									radius: 3
									color: "white"
									visible: stationRow.current
								}
							}

							Column
							{
								anchors.left: stationMark.right
								anchors.leftMargin: 12
								anchors.right: parent.right
								anchors.rightMargin: 8
								anchors.verticalCenter: parent.verticalCenter

								Text
								{
									width: parent.width
									text: stationRow.modelData.name
									color: "white"
									font.pixelSize: 14
									font.bold: stationRow.current
									elide: Text.ElideRight
								}

								Text
								{
									width: parent.width
									text: stationRow.modelData.note
									color: Qt.rgba(1, 1, 1, 0.55)
									font.pixelSize: 11
									elide: Text.ElideRight
								}
							}
						}
					}

					Text
					{
						width: parent.width
						topPadding: 6
						bottomPadding: 4

						text: "Not the one you want? Paste any stream under Link."
						color: Qt.rgba(1, 1, 1, 0.5)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
					}
				}

				// YouTube: any video, mix or live stream, played as audio through yt-dlp.
				Column
				{
					width: parent.width
					spacing: 6
					visible: AppSettings.musicSource === "youtube"

					LinkField
					{
						id: youtubeField

						width: parent.width
						text: AppSettings.youtubeUrl
						placeholderText: "YouTube link: a video, a mix or a live stream"

						onCommitted: (value) => AppSettings.youtubeUrl = value
					}

					Text
					{
						width: parent.width

						text: MusicPlayer.youtube.statusText
						textFormat: Text.PlainText
						color: MusicPlayer.youtube.available ? Qt.rgba(1, 1, 1, 0.5) : "#ffcf8a"
						font.pixelSize: 11
						wrapMode: Text.WrapAnywhere
					}

					PanelButton
					{
						width: parent.width
						visible: !MusicPlayer.youtube.available || MusicPlayer.youtube.installing
						enabled: !MusicPlayer.youtube.installing

						label: MusicPlayer.youtube.installing
							? "Downloading yt-dlp… " + Math.round(MusicPlayer.youtube.installProgress * 100) + "%"
							: "Download yt-dlp (about 18 MB)"

						onClicked:
							MusicPlayer.youtube.install()
					}

					Text
					{
						width: parent.width
						bottomPadding: 4

						text: "yt-dlp comes from its official GitHub releases and is checked against their published checksums. It only reads the link you give it."
						color: Qt.rgba(1, 1, 1, 0.4)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
						visible: !MusicPlayer.youtube.available
					}
				}

				// Spotify: remote control of the user's own Spotify, which is all Spotify
				// allows. Said plainly so nobody expects the sound to come from here.
				Column
				{
					width: parent.width
					spacing: 6
					visible: AppSettings.musicSource === "spotify"

					Text
					{
						width: parent.width

						text: MusicPlayer.spotify.statusText
						textFormat: Text.PlainText
						color: MusicPlayer.spotify.connected ? "#b6f0b6" : Qt.rgba(1, 1, 1, 0.75)
						font.pixelSize: 12
						wrapMode: Text.WordWrap
					}

					PanelButton
					{
						width: parent.width
						enabled: !MusicPlayer.spotify.connecting
							&& (MusicPlayer.spotify.connected || MusicPlayer.spotify.clientId.length > 0)

						label: MusicPlayer.spotify.connected
							? "Disconnect Spotify"
							: MusicPlayer.spotify.connecting ? "Waiting for the browser…" : "Connect Spotify"

						onClicked:
						{
							if (MusicPlayer.spotify.connected)
								MusicPlayer.spotify.disconnectAccount()
							else
								MusicPlayer.spotify.connectAccount()
						}
					}

					LinkField
					{
						id: spotifyField

						width: parent.width
						visible: MusicPlayer.spotify.connected

						text: AppSettings.spotifyUri
						placeholderText: "Playlist, album or song link (empty: resume Spotify)"

						onCommitted: (value) =>
						{
							let uri = MusicPlayer.spotify.toUri(value)

							spotifyField.rejected = value.length > 0 && uri.length === 0

							if (!spotifyField.rejected)
								AppSettings.spotifyUri = uri
						}
					}

					Text
					{
						width: parent.width
						visible: spotifyField.visible && spotifyField.rejected

						text: "That is not a Spotify link. Use Share → Copy link in Spotify."
						color: "#ff8a8a"
						font.pixelSize: 11
						wrapMode: Text.WordWrap
					}

					Text
					{
						width: parent.width

						text: "Plays through your own Spotify app, on this computer or your phone, so keep it open. Spotify only allows this on Premium accounts."
						color: Qt.rgba(1, 1, 1, 0.45)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
					}

					// Only needed when this build carries no Client ID of its own, or to use
					// a different one.
					LinkField
					{
						id: clientIdField

						width: parent.width
						visible: MusicPlayer.spotify.builtInClientId.length === 0 || AppSettings.spotifyClientId.length > 0

						text: AppSettings.spotifyClientId
						placeholderText: "Spotify Client ID"

						onCommitted: (value) => AppSettings.spotifyClientId = value
					}

					TextEdit
					{
						width: parent.width
						visible: clientIdField.visible

						text: "To get one: create an app at developer.spotify.com/dashboard, pick Web API, add the redirect URI " + MusicPlayer.spotify.redirectUri + " and paste the app's Client ID above."
						readOnly: true
						selectByMouse: true
						color: Qt.rgba(1, 1, 1, 0.45)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
					}
				}

				// Link: any HTTP/Icecast/Shoutcast audio stream.
				Column
				{
					width: parent.width
					spacing: 6
					visible: AppSettings.musicSource === "custom"

					LinkField
					{
						id: streamField

						width: parent.width
						text: AppSettings.streamUrl
						placeholderText: "Stream URL (Icecast, Shoutcast, MP3/AAC over HTTP)"

						onCommitted: (value) => AppSettings.streamUrl = value
					}
				}

				Text
				{
					width: parent.width
					topPadding: 4
					bottomPadding: 4

					text: MusicPlayer.statusText
					color: MusicPlayer.failed ? "#ff8a8a" : Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 12
					wrapMode: Text.WordWrap
				}

				SliderSetting
				{
					// Spotify has its own volume; this one would change nothing.
					visible: AppSettings.musicSource !== "spotify"
					label: "Music volume"
					value: AppSettings.musicVolume

					onValueModified: (newValue) => AppSettings.musicVolume = newValue
				}

				ToggleSetting
				{
					label: "Play only during focus"
					checked: AppSettings.musicFollowsFocus
					accentColor: rootPanel.themeColor

					onToggleRequested: (wanted) => AppSettings.musicFollowsFocus = wanted
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					text: "WINDOW"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 14
					bottomPadding: 6
				}

				ToggleSetting
				{
					label: "Floating timer when minimised"
					checked: AppSettings.miniTimer
					accentColor: rootPanel.themeColor

					onToggleRequested: (wanted) => AppSettings.miniTimer = wanted
				}

				ToggleSetting
				{
					label: "Close button hides to the tray"
					checked: AppSettings.closeMinimizes
					accentColor: rootPanel.themeColor

					onToggleRequested: (wanted) => AppSettings.closeMinimizes = wanted
				}

				Text
				{
					width: parent.width
					bottomPadding: 6

					text:
					{
						if (!AppSettings.closeMinimizes)
							return "Closing quits pomodoro and ends the current session."

						if (TrayIcon.available)
							return "Closing puts pomodoro in the system tray with the timer still running. Click the tray icon to bring it back, or use its menu."

						// Said plainly rather than hidden, because the fallback behaves
						// differently from what the switch above describes.
						return "This desktop has no system tray, so closing will minimise the window instead. On GNOME a tray needs the AppIndicator extension."
					}

					color: TrayIcon.available || !AppSettings.closeMinimizes
						? Qt.rgba(1, 1, 1, 0.5)
						: "#ffcf8a"

					font.pixelSize: 11
					wrapMode: Text.WordWrap
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					text: "ABOUT"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 14
					bottomPadding: 6
				}

				Text
				{
					width: parent.width
					bottomPadding: 8

					text: UpdateChecker.statusText
					color: UpdateChecker.updateAvailable
						? "#b6f0b6"
						: UpdateChecker.status === UpdateChecker.Failed
							? "#ff8a8a"
							: Qt.rgba(1, 1, 1, 0.6)

					font.pixelSize: 12
					wrapMode: Text.WordWrap
				}

				// One button with two jobs: it looks for a newer release, and once it has
				// found one it becomes the way to install it -- in place where this copy
				// knows how to replace itself, through the release page where it does not.
				Button
				{
					id: updateBtn

					width: parent.width
					height: 40

					enabled: !UpdateChecker.busy
					opacity: updateBtn.enabled ? 1.0 : 0.5

					background: Rectangle
					{
						radius: 8
						color: UpdateChecker.updateAvailable
							? Qt.rgba(0.36, 0.67, 0.36, updateBtn.hovered ? 0.55 : 0.4)
							: updateBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)

						border.color: Qt.rgba(1, 1, 1, 0.25)
						border.width: 1

						Behavior on color
						{
							ColorAnimation { duration: 150 }
						}
					}

					contentItem: Text
					{
						text:
						{
							if (UpdateChecker.status === UpdateChecker.Checking)
								return "Checking…"

							if (UpdateChecker.status === UpdateChecker.Downloading)
								return "Downloading… " + Math.round(UpdateChecker.downloadProgress * 100) + "%"

							if (!UpdateChecker.updateAvailable)
								return "Check for updates"

							return UpdateChecker.canInstall
								? "Update to " + UpdateChecker.latestVersion
								: "Get version " + UpdateChecker.latestVersion
						}

						color: "white"
						font.pixelSize: 14

						horizontalAlignment: Text.AlignHCenter
						verticalAlignment: Text.AlignVCenter
					}

					onClicked:
					{
						SoundPlayer.playClick()

						if (UpdateChecker.updateAvailable)
							UpdateChecker.installUpdate()
						else
							UpdateChecker.check()
					}
				}

				Item
				{
					width: 1
					height: 16
				}

				Button
				{
					id: quitBtn

					property bool armed: false

					width: parent.width
					height: 40

					background: Rectangle
					{
						radius: 8
						color: quitBtn.armed
							? "#d91629"
							: quitBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)

						border.color: Qt.rgba(1, 1, 1, 0.25)
						border.width: 1

						Behavior on color
						{
							ColorAnimation { duration: 150 }
						}
					}

					contentItem: Text
					{
						text: quitBtn.armed ? "Really quit?" : "Quit pomodoro"
						color: "white"
						font.pixelSize: 14

						horizontalAlignment: Text.AlignHCenter
						verticalAlignment: Text.AlignVCenter
					}

					// Two step, because quitting mid session throws that session away.
					onClicked:
					{
						if (quitBtn.armed)
							Qt.quit()
						else
						{
							quitBtn.armed = true
							disarmQuit.restart()
						}
					}

					Timer
					{
						id: disarmQuit
						interval: 4000

						onTriggered:
							quitBtn.armed = false
					}
				}

				Item
				{
					width: 1
					height: 16
				}

				Button
				{
					id: defaultsBtn

					width: parent.width
					height: 40

					background: Rectangle
					{
						radius: 8
						color: defaultsBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
						border.color: Qt.rgba(1, 1, 1, 0.25)
						border.width: 1

						Behavior on color
						{
							ColorAnimation { duration: 150 }
						}
					}

					contentItem: Text
					{
						text: "Restore defaults"
						color: "white"
						font.pixelSize: 14

						horizontalAlignment: Text.AlignHCenter
						verticalAlignment: Text.AlignVCenter
					}

					onClicked:
						AppSettings.restoreDefaults()
				}
			}
		}
	}
}
