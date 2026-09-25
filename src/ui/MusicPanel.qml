// Delegates reach rootPanel from inside nested components.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import Pomodoro

// The music panel: a small window inside the app, opened from the title bar, for picking
// and controlling what plays without going through the settings.
//
// On top: the four sources as icons, then what is playing now -- cover, title, and
// previous / play-pause / next. Below, the chosen source's own view: the station list,
// YouTube search and playlists, the user's Spotify library and search, or the link field.
Item
{
	id: rootPanel

	required property var stations

	property bool open: false
	property color themeColor: "#12130F"

	// Skip on the radio list is a change of station, which only Main.qml can make.
	signal skipRequested(int step)

	readonly property bool typing: youtubeSearch.activeFocus || spotifySearch.activeFocus
		|| linkField.activeFocus || clientIdField.activeFocus

	readonly property string source: AppSettings.musicSource

	visible: rootPanel.open || card.opacity > 0

	// Clicking anywhere outside the card closes it, like a menu.
	MouseArea
	{
		anchors.fill: parent
		enabled: rootPanel.open

		onClicked:
			rootPanel.open = false
	}

	Rectangle
	{
		id: card

		x: 12
		y: 46

		width: Math.min(460, rootPanel.width - 24)
		height: Math.min(620, rootPanel.height - 58)
		radius: 16

		color: Qt.darker(rootPanel.themeColor, 1.45)
		border.color: Qt.rgba(1, 1, 1, 0.18)
		border.width: 1

		opacity: rootPanel.open ? 1.0 : 0.0
		scale: rootPanel.open ? 1.0 : 0.96
		transformOrigin: Item.TopLeft

		Behavior on opacity
		{
			NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
		}

		Behavior on scale
		{
			NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
		}

		Behavior on color
		{
			ColorAnimation { duration: 500; easing.type: Easing.InOutQuad }
		}

		// Clicks inside the card stay inside the card.
		MouseArea
		{
			anchors.fill: parent
		}

		Item
		{
			id: header

			anchors.top: parent.top
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.margins: 14

			height: 44

			SourceTabs
			{
				anchors.left: parent.left
				anchors.verticalCenter: parent.verticalCenter

				current: rootPanel.source

				onPicked: (key) => AppSettings.musicSource = key
			}

			Button
			{
				id: closeBtn

				anchors.right: parent.right
				anchors.verticalCenter: parent.verticalCenter

				width: 34
				height: 34

				background: Rectangle
				{
					radius: 8
					color: closeBtn.hovered ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
				}

				icon.source: "assets/icons/close.svg"
				icon.color: "white"
				icon.width: 14
				icon.height: 14

				onClicked:
					rootPanel.open = false
			}
		}

		// ---------------------------------------------------------------- now playing

		Rectangle
		{
			id: nowPlaying

			anchors.top: header.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.margins: 14

			height: 84
			radius: 12
			color: Qt.rgba(0, 0, 0, 0.18)

			Rectangle
			{
				id: art

				anchors.left: parent.left
				anchors.leftMargin: 10
				anchors.verticalCenter: parent.verticalCenter

				width: 64
				height: 64
				radius: 10
				clip: true

				color: Qt.rgba(1, 1, 1, 0.08)

				Image
				{
					id: artImage

					anchors.fill: parent

					source: MusicPlayer.artUrl
					fillMode: Image.PreserveAspectCrop
					asynchronous: true
					visible: status === Image.Ready
				}

				// No cover for a radio stream, or not loaded yet: the source's own icon.
				Image
				{
					anchors.centerIn: parent

					width: 30
					height: 30

					visible: !artImage.visible
					source: rootPanel.iconFor(rootPanel.source)
					sourceSize.width: 64
					sourceSize.height: 64
				}
			}

			Column
			{
				anchors.left: art.right
				anchors.leftMargin: 12
				anchors.right: controls.left
				anchors.rightMargin: 8
				anchors.verticalCenter: parent.verticalCenter

				spacing: 3

				Text
				{
					width: parent.width

					text:
					{
						if (MusicPlayer.title.length > 0)
							return MusicPlayer.title

						if (MusicPlayer.stationName.length > 0)
							return MusicPlayer.stationName

						return MusicPlayer.active ? "Starting…" : "Nothing playing"
					}

					textFormat: Text.PlainText
					color: "white"
					font.pixelSize: 14
					font.bold: true
					elide: Text.ElideRight
					maximumLineCount: 1
				}

				Text
				{
					width: parent.width

					text: MusicPlayer.failed || MusicPlayer.status === MusicPlayer.Connecting
							|| MusicPlayer.status === MusicPlayer.Reconnecting
						? MusicPlayer.statusText
						: MusicPlayer.title.length > 0 && MusicPlayer.stationName.length > 0
							? MusicPlayer.stationName
							: MusicPlayer.genre

					textFormat: Text.PlainText
					color: MusicPlayer.failed ? "#ff9a9a" : Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 12
					elide: Text.ElideRight
					maximumLineCount: 2
					wrapMode: Text.Wrap
				}
			}

			Row
			{
				id: controls

				anchors.right: parent.right
				anchors.rightMargin: 10
				anchors.verticalCenter: parent.verticalCenter

				spacing: 4

				TransportButton
				{
					iconSource: "assets/icons/skipPrevious.svg"
					enabled: rootPanel.canSkip

					onClicked:
						rootPanel.skipRequested(-1)
				}

				TransportButton
				{
					primary: true
					iconSource: MusicPlayer.active ? "assets/icons/pauseSolid.svg" : "assets/icons/playSolid.svg"
					iconColor: rootPanel.themeColor

					onClicked:
						MusicPlayer.toggle()
				}

				TransportButton
				{
					iconSource: "assets/icons/skipNext.svg"
					enabled: rootPanel.canSkip

					onClicked:
						rootPanel.skipRequested(1)
				}
			}
		}

		// ---------------------------------------------------------------- volume

		// The music's volume and whether it plays only while focusing -- the two things
		// changed most often, kept next to the music rather than in the settings.
		Item
		{
			id: mixer

			anchors.top: nowPlaying.bottom
			anchors.topMargin: 6
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.leftMargin: 14
			anchors.rightMargin: 14

			height: 36

			Image
			{
				id: volumeIcon

				anchors.left: parent.left
				anchors.leftMargin: 4
				anchors.verticalCenter: parent.verticalCenter

				width: 18
				height: 18
				source: AppSettings.musicVolume === 0 ? "assets/icons/volumeOff.svg" : "assets/icons/volume.svg"
				sourceSize.width: 36
				sourceSize.height: 36
				opacity: 0.8
			}

			Slider
			{
				id: volumeSlider

				anchors.left: volumeIcon.right
				anchors.leftMargin: 8
				anchors.right: focusChip.left
				anchors.rightMargin: 12
				anchors.verticalCenter: parent.verticalCenter

				// Spotify has its own volume; this one would change nothing there.
				visible: rootPanel.source !== "spotify"

				from: 0
				to: 1
				value: AppSettings.musicVolume

				onMoved:
					AppSettings.musicVolume = volumeSlider.value

				background: Rectangle
				{
					x: volumeSlider.leftPadding
					y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
					width: volumeSlider.availableWidth
					height: 4
					radius: 2
					color: Qt.rgba(1, 1, 1, 0.2)

					Rectangle
					{
						width: volumeSlider.visualPosition * parent.width
						height: parent.height
						radius: 2
						color: "white"
					}
				}

				handle: Rectangle
				{
					x: volumeSlider.leftPadding + volumeSlider.visualPosition * (volumeSlider.availableWidth - width)
					y: volumeSlider.topPadding + volumeSlider.availableHeight / 2 - height / 2
					width: 14
					height: 14
					radius: 7
					color: "white"
				}
			}

			Text
			{
				anchors.left: volumeIcon.right
				anchors.leftMargin: 8
				anchors.verticalCenter: parent.verticalCenter

				visible: !volumeSlider.visible
				text: "Volume is set in Spotify"
				color: Qt.rgba(1, 1, 1, 0.5)
				font.pixelSize: 12
			}

			Chip
			{
				id: focusChip

				anchors.right: parent.right
				anchors.verticalCenter: parent.verticalCenter

				label: "Focus only"
				selected: AppSettings.musicFollowsFocus

				ToolTip.visible: hovered
				ToolTip.delay: 500
				ToolTip.text: "Play only while a focus session is running, and pause for breaks"

				onClicked:
					AppSettings.musicFollowsFocus = !AppSettings.musicFollowsFocus
			}
		}

		// ---------------------------------------------------------------- body

		Item
		{
			id: body

			anchors.top: mixer.bottom
			anchors.bottom: parent.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.margins: 14

			// Radio: every station as a tile.
			GridView
			{
				id: stationGrid

				anchors.fill: parent
				visible: rootPanel.source === "radio"

				clip: true
				boundsBehavior: Flickable.StopAtBounds
				cellWidth: width / 2
				cellHeight: 64

				model: rootPanel.stations

				ScrollBar.vertical: SlimScrollBar {}

				delegate: Item
				{
					id: stationCell

					required property var modelData
					required property int index

					readonly property bool current: AppSettings.radioUrl === stationCell.modelData.url

					width: stationGrid.cellWidth
					height: stationGrid.cellHeight

					Rectangle
					{
						anchors.fill: parent
						anchors.margins: 3
						radius: 10

						color: stationCell.current
							? Qt.rgba(1, 1, 1, 0.24)
							: stationHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.05)

						border.color: stationCell.current ? Qt.rgba(1, 1, 1, 0.6) : "transparent"
						border.width: 1

						Image
						{
							id: stationIcon

							anchors.left: parent.left
							anchors.leftMargin: 10
							anchors.verticalCenter: parent.verticalCenter

							width: 20
							height: 20
							source: "assets/icons/radio.svg"
							sourceSize.width: 40
							sourceSize.height: 40
							opacity: stationCell.current ? 1.0 : 0.6
						}

						Column
						{
							anchors.left: stationIcon.right
							anchors.leftMargin: 10
							anchors.right: parent.right
							anchors.rightMargin: 8
							anchors.verticalCenter: parent.verticalCenter

							Text
							{
								width: parent.width
								text: stationCell.modelData.name
								color: "white"
								font.pixelSize: 13
								font.bold: stationCell.current
								elide: Text.ElideRight
							}

							Text
							{
								width: parent.width
								text: stationCell.modelData.note
								color: Qt.rgba(1, 1, 1, 0.55)
								font.pixelSize: 11
								elide: Text.ElideRight
							}
						}

						HoverHandler
						{
							id: stationHover
							cursorShape: Qt.PointingHandCursor
						}

						// Picking a station plays it; that is almost always why it was picked.
						TapHandler
						{
							onTapped:
							{
								AppSettings.radioUrl = stationCell.modelData.url

								if (!MusicPlayer.active)
									MusicPlayer.play()
							}
						}
					}
				}
			}

			// YouTube: search videos or playlists, open a playlist, tap to play.
			Item
			{
				anchors.fill: parent
				visible: rootPanel.source === "youtube"

				Column
				{
					anchors.centerIn: parent
					width: parent.width - 20
					spacing: 10
					visible: !MusicPlayer.youtube.available

					Text
					{
						width: parent.width
						text: MusicPlayer.youtube.statusText
						color: "white"
						font.pixelSize: 13
						wrapMode: Text.WordWrap
						horizontalAlignment: Text.AlignHCenter
					}

					PillButton
					{
						anchors.horizontalCenter: parent.horizontalCenter
						label: MusicPlayer.youtube.installing
							? "Downloading… " + Math.round(MusicPlayer.youtube.installProgress * 100) + "%"
							: "Download yt-dlp"
						enabled: !MusicPlayer.youtube.installing

						onClicked:
							MusicPlayer.youtube.install()
					}
				}

				Column
				{
					id: youtubeTop

					anchors.top: parent.top
					anchors.left: parent.left
					anchors.right: parent.right

					spacing: 8
					visible: MusicPlayer.youtube.available

					SearchField
					{
						id: youtubeSearch

						width: parent.width
						placeholderText: rootPanel.youtubePlaylists
							? "Search YouTube playlists"
							: "Search YouTube: a song, a mix, a live stream"

						onSubmitted: (query) =>
						{
							rootPanel.youtubeQuery = query
							MusicPlayer.youtube.search(query, rootPanel.youtubePlaylists)
						}
					}

					Row
					{
						spacing: 6

						Chip
						{
							label: "Videos"
							selected: !rootPanel.youtubePlaylists

							onClicked:
							{
								rootPanel.youtubePlaylists = false
								rootPanel.searchYouTubeAgain()
							}
						}

						Chip
						{
							label: "Playlists"
							selected: rootPanel.youtubePlaylists

							onClicked:
							{
								rootPanel.youtubePlaylists = true
								rootPanel.searchYouTubeAgain()
							}
						}
					}

					// Inside an opened playlist: the way back, its name, and play all.
					Row
					{
						width: parent.width
						spacing: 8
						visible: MusicPlayer.youtube.resultsTitle.length > 0

						Chip
						{
							label: "‹ Back"

							onClicked:
								rootPanel.searchYouTubeAgain()
						}

						Text
						{
							anchors.verticalCenter: parent.verticalCenter
							width: parent.width - 180
							text: MusicPlayer.youtube.resultsTitle
							textFormat: Text.PlainText
							color: "white"
							font.pixelSize: 13
							font.bold: true
							elide: Text.ElideRight
						}

						Chip
						{
							label: "▶ Play all"
							selected: true

							onClicked:
								rootPanel.playYouTube(0)
						}
					}
				}

				ResultList
				{
					anchors.top: youtubeTop.bottom
					anchors.topMargin: 8
					anchors.bottom: parent.bottom
					anchors.left: parent.left
					anchors.right: parent.right

					visible: MusicPlayer.youtube.available
					model: MusicPlayer.youtube.results
					busy: MusicPlayer.youtube.searching
					message: MusicPlayer.youtube.resultsError.length > 0
						? MusicPlayer.youtube.resultsError
						: "Search for anything you like to work to."
					wideImages: true
					suggestions: [ "lofi hip hop", "study music", "jazz for work", "rain sounds", "piano focus", "synthwave" ]

					onSuggestionPicked: (text) =>
					{
						youtubeSearch.text = text
						rootPanel.youtubeQuery = text
						MusicPlayer.youtube.search(text, rootPanel.youtubePlaylists)
					}

					onActivated: (index, item) =>
					{
						if (item.kind === "playlist")
							MusicPlayer.youtube.openPlaylist(item.url, item.title)
						else
							rootPanel.playYouTube(index)
					}
				}
			}

			// Spotify: the user's library and search, played through their own Spotify.
			Item
			{
				anchors.fill: parent
				visible: rootPanel.source === "spotify"

				Column
				{
					anchors.centerIn: parent
					width: parent.width - 20
					spacing: 10
					visible: !MusicPlayer.spotify.connected

					Text
					{
						width: parent.width
						text: MusicPlayer.spotify.clientId.length > 0
							? "Connect your Spotify account to search and play your music here. Spotify Premium is needed."
							: "Spotify needs a Client ID first."
						color: "white"
						font.pixelSize: 13
						wrapMode: Text.WordWrap
						horizontalAlignment: Text.AlignHCenter
					}

					Text
					{
						width: parent.width
						visible: MusicPlayer.spotify.statusText.length > 0 && !MusicPlayer.spotify.connecting
							&& MusicPlayer.spotify.clientId.length > 0
						text: MusicPlayer.spotify.statusText
						textFormat: Text.PlainText
						color: Qt.rgba(1, 1, 1, 0.6)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
						horizontalAlignment: Text.AlignHCenter
					}

					// Only needed when the build carries no Client ID of its own.
					SearchField
					{
						id: clientIdField

						width: parent.width
						visible: MusicPlayer.spotify.builtInClientId.length === 0
						text: AppSettings.spotifyClientId
						placeholderText: "Spotify Client ID, then Enter"
						iconSource: "assets/icons/spotify.svg"

						onSubmitted: (value) => AppSettings.spotifyClientId = value
					}

					TextEdit
					{
						width: parent.width
						visible: clientIdField.visible && MusicPlayer.spotify.clientId.length === 0

						text: "Create an app at developer.spotify.com/dashboard, choose Web API, add the redirect URI " + MusicPlayer.spotify.redirectUri + " and paste its Client ID above."
						readOnly: true
						selectByMouse: true
						color: Qt.rgba(1, 1, 1, 0.5)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
						horizontalAlignment: Text.AlignHCenter
					}

					PillButton
					{
						anchors.horizontalCenter: parent.horizontalCenter
						label: MusicPlayer.spotify.connecting ? "Waiting for the browser…" : "Connect Spotify"
						enabled: MusicPlayer.spotify.clientId.length > 0 && !MusicPlayer.spotify.connecting

						onClicked:
							MusicPlayer.spotify.connectAccount()
					}
				}

				Column
				{
					id: spotifyTop

					anchors.top: parent.top
					anchors.left: parent.left
					anchors.right: parent.right

					spacing: 8
					visible: MusicPlayer.spotify.connected

					// Signed in before the library was part of it: one more approval.
					Rectangle
					{
						width: parent.width
						height: 44
						radius: 10
						visible: MusicPlayer.spotify.needsReconnect
						color: Qt.rgba(1, 1, 1, 0.1)

						Text
						{
							anchors.left: parent.left
							anchors.leftMargin: 12
							anchors.right: reconnectBtn.left
							anchors.rightMargin: 8
							anchors.verticalCenter: parent.verticalCenter

							text: "Reconnect once to see your playlists and history."
							color: "white"
							font.pixelSize: 12
							wrapMode: Text.WordWrap
						}

						PillButton
						{
							id: reconnectBtn

							anchors.right: parent.right
							anchors.rightMargin: 6
							anchors.verticalCenter: parent.verticalCenter

							label: MusicPlayer.spotify.connecting ? "Waiting…" : "Reconnect"
							enabled: !MusicPlayer.spotify.connecting

							onClicked:
								MusicPlayer.spotify.connectAccount()
						}
					}

					Row
					{
						width: parent.width
						spacing: 8

						Text
						{
							anchors.verticalCenter: parent.verticalCenter
							width: parent.width - disconnectChip.width - 8

							text: MusicPlayer.spotify.accountName.length > 0
								? "Signed in as " + MusicPlayer.spotify.accountName
								: "Signed in to Spotify"
							textFormat: Text.PlainText
							color: "#b6f0b6"
							font.pixelSize: 12
							elide: Text.ElideRight
						}

						Chip
						{
							id: disconnectChip

							label: "Disconnect"

							onClicked:
							{
								rootPanel.spotifySection = ""
								MusicPlayer.spotify.disconnectAccount()
							}
						}
					}

					SearchField
					{
						id: spotifySearch

						width: parent.width
						placeholderText: "Search Spotify: songs, playlists, albums, artists"

						onSubmitted: (query) =>
						{
							rootPanel.spotifySection = "search"
							MusicPlayer.spotify.search(query)
						}
					}

					Row
					{
						spacing: 6

						Repeater
						{
							model: [
								{ key: "playlists", label: "Your playlists" },
								{ key: "liked", label: "Liked" },
								{ key: "recent", label: "Recent" },
								{ key: "top", label: "Most played" }
							]

							delegate: Chip
							{
								required property var modelData

								label: modelData.label
								selected: rootPanel.spotifySection === modelData.key

								onClicked:
									rootPanel.loadSpotify(modelData.key)
							}
						}
					}
				}

				ResultList
				{
					anchors.top: spotifyTop.bottom
					anchors.topMargin: 8
					anchors.bottom: parent.bottom
					anchors.left: parent.left
					anchors.right: parent.right

					visible: MusicPlayer.spotify.connected
					model: MusicPlayer.spotify.results
					busy: MusicPlayer.spotify.searching
					message: MusicPlayer.spotify.resultsError.length > 0
						? MusicPlayer.spotify.resultsError
						: "Search, or pick one of your lists above."

					onActivated: (index, item) =>
						MusicPlayer.playSpotifyResult(index)
				}
			}

			// Link: any stream of the user's own.
			Column
			{
				anchors.fill: parent
				spacing: 10
				visible: rootPanel.source === "custom"

				Text
				{
					width: parent.width
					text: "Paste the address of any internet radio or audio stream (Icecast, Shoutcast, MP3 or AAC over HTTP), then press Enter."
					color: Qt.rgba(1, 1, 1, 0.75)
					font.pixelSize: 12
					wrapMode: Text.WordWrap
				}

				SearchField
				{
					id: linkField

					width: parent.width
					text: AppSettings.streamUrl
					placeholderText: "https://…"
					iconSource: "assets/icons/plus.svg"

					onSubmitted: (value) =>
					{
						AppSettings.streamUrl = value
						MusicPlayer.play()
					}
				}
			}
		}
	}

	// ---------------------------------------------------------------- state and helpers

	property bool youtubePlaylists: false
	property string youtubeQuery: ""
	property string spotifySection: ""

	readonly property bool canSkip: rootPanel.source === "radio" || MusicPlayer.canSkip

	function iconFor(key)
	{
		switch (key)
		{
			case "youtube": return "assets/icons/youtube.svg"
			case "spotify": return "assets/icons/spotify.svg"
			case "custom": return "assets/icons/plus.svg"
			default: return "assets/icons/radio.svg"
		}
	}

	function searchYouTubeAgain()
	{
		if (rootPanel.youtubeQuery.length > 0)
			MusicPlayer.youtube.search(rootPanel.youtubeQuery, rootPanel.youtubePlaylists)
	}

	function playYouTube(index)
	{
		MusicPlayer.playYouTubeQueue(MusicPlayer.youtube.results, index)
	}

	function loadSpotify(section)
	{
		rootPanel.spotifySection = section
		MusicPlayer.spotify.loadLibrary(section)
	}

	// The first visit to a tab shows something rather than a blank: the user's Spotify
	// playlists, or the YouTube playlist they pasted in the settings.
	onOpenChanged:
	{
		rootPanel.fillSpotify()
		rootPanel.fillYouTube()
	}

	onSourceChanged:
	{
		rootPanel.fillSpotify()
		rootPanel.fillYouTube()
	}

	function fillYouTube()
	{
		if (rootPanel.open && rootPanel.source === "youtube" && MusicPlayer.youtube.available
			&& MusicPlayer.youtube.results.length === 0 && !MusicPlayer.youtube.searching
			&& AppSettings.youtubeUrl.indexOf("list=") >= 0)
		{
			MusicPlayer.youtube.openPlaylist(AppSettings.youtubeUrl, "Your playlist")
		}
	}

	function fillSpotify()
	{
		if (rootPanel.open && rootPanel.source === "spotify" && MusicPlayer.spotify.connected
			&& !MusicPlayer.spotify.needsReconnect && rootPanel.spotifySection === "")
		{
			rootPanel.loadSpotify("playlists")
		}
	}

	Connections
	{
		target: MusicPlayer.spotify

		function onStateChanged()
		{
			rootPanel.fillSpotify()
		}
	}

	// ---------------------------------------------------------------- components

	component TransportButton: Button
	{
		id: transportBtn

		property string iconSource: ""
		property color iconColor: "white"
		property bool primary: false

		width: transportBtn.primary ? 44 : 34
		height: transportBtn.width

		opacity: transportBtn.enabled ? 1.0 : 0.3

		background: Rectangle
		{
			radius: width / 2
			color: transportBtn.primary
				? (transportBtn.hovered ? "#ffffff" : Qt.rgba(1, 1, 1, 0.92))
				: (transportBtn.hovered ? Qt.rgba(1, 1, 1, 0.18) : "transparent")
		}

		icon.source: transportBtn.iconSource
		icon.color: transportBtn.iconColor
		icon.width: transportBtn.primary ? 20 : 18
		icon.height: transportBtn.primary ? 20 : 18
	}

	component PillButton: Button
	{
		id: pillBtn

		property string label: ""

		height: 34
		leftPadding: 16
		rightPadding: 16
		opacity: pillBtn.enabled ? 1.0 : 0.5

		background: Rectangle
		{
			radius: height / 2
			color: pillBtn.hovered ? "#ffffff" : Qt.rgba(1, 1, 1, 0.9)
		}

		contentItem: Text
		{
			text: pillBtn.label
			color: "#333333"
			font.pixelSize: 13
			font.bold: true
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
	}

	component Chip: Button
	{
		id: chip

		property string label: ""
		property bool selected: false

		height: 28
		leftPadding: 12
		rightPadding: 12

		background: Rectangle
		{
			radius: height / 2
			color: chip.selected
				? Qt.rgba(1, 1, 1, 0.9)
				: chip.hovered ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(1, 1, 1, 0.08)
		}

		contentItem: Text
		{
			text: chip.label
			color: chip.selected ? "#333333" : "white"
			font.pixelSize: 12
			font.bold: chip.selected
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
	}

	// A search box: the icon on the left, Enter to go.
	component SearchField: TextField
	{
		id: field

		property string iconSource: "assets/icons/search.svg"

		signal submitted(string value)

		height: 38
		leftPadding: 36

		color: "white"
		placeholderTextColor: Qt.rgba(1, 1, 1, 0.45)
		font.pixelSize: 13
		selectByMouse: true

		background: Rectangle
		{
			radius: 10
			color: Qt.rgba(0, 0, 0, 0.22)
			border.color: field.activeFocus ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.18)
			border.width: 1

			Image
			{
				anchors.left: parent.left
				anchors.leftMargin: 11
				anchors.verticalCenter: parent.verticalCenter

				width: 16
				height: 16
				source: field.iconSource
				sourceSize.width: 32
				sourceSize.height: 32
				opacity: 0.7
			}
		}

		onAccepted:
			if (field.text.trim().length > 0)
				field.submitted(field.text.trim())
	}

	// A list of songs, videos or playlists with their pictures. Tapping a row plays it, or
	// opens it when it is a list of its own.
	component ResultList: ListView
	{
		id: list

		property bool busy: false
		property string message: ""
		property bool wideImages: false
		property var suggestions: []

		signal activated(int index, var item)
		signal suggestionPicked(string text)

		clip: true
		spacing: 2
		boundsBehavior: Flickable.StopAtBounds

		ScrollBar.vertical: SlimScrollBar {}

		delegate: Rectangle
		{
			id: resultRow

			required property var modelData
			required property int index

			readonly property bool collection: resultRow.modelData.kind !== "track"
				&& resultRow.modelData.kind !== "video"

			width: ListView.view.width
			height: 58
			radius: 8

			color: resultHover.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

			Rectangle
			{
				id: thumb

				anchors.left: parent.left
				anchors.leftMargin: 4
				anchors.verticalCenter: parent.verticalCenter

				width: list.wideImages ? 80 : 46
				height: list.wideImages ? 45 : 46
				radius: resultRow.modelData.kind === "artist" ? height / 2 : 6
				clip: true

				color: Qt.rgba(1, 1, 1, 0.08)

				Image
				{
					anchors.fill: parent
					source: resultRow.modelData.image || ""
					fillMode: Image.PreserveAspectCrop
					asynchronous: true
					sourceSize.width: 160
				}

				// The play mark over the picture while hovered.
				Rectangle
				{
					anchors.fill: parent
					color: Qt.rgba(0, 0, 0, 0.45)
					visible: resultHover.hovered && !resultRow.collection

					Image
					{
						anchors.centerIn: parent
						width: 18
						height: 18
						source: "assets/icons/playSolid.svg"
						sourceSize.width: 36
						sourceSize.height: 36
					}
				}
			}

			Column
			{
				anchors.left: thumb.right
				anchors.leftMargin: 10
				anchors.right: parent.right
				anchors.rightMargin: resultRow.collection ? 26 : 8
				anchors.verticalCenter: parent.verticalCenter

				spacing: 2

				Text
				{
					width: parent.width
					text: resultRow.modelData.title
					textFormat: Text.PlainText
					color: "white"
					font.pixelSize: 13
					elide: Text.ElideRight
				}

				Text
				{
					width: parent.width
					text: resultRow.modelData.subtitle
					textFormat: Text.PlainText
					color: resultRow.modelData.live ? "#ff9a9a" : Qt.rgba(1, 1, 1, 0.55)
					font.pixelSize: 11
					elide: Text.ElideRight
				}
			}

			Text
			{
				anchors.right: parent.right
				anchors.rightMargin: 10
				anchors.verticalCenter: parent.verticalCenter

				visible: resultRow.collection
				text: "›"
				color: Qt.rgba(1, 1, 1, 0.6)
				font.pixelSize: 20
			}

			HoverHandler
			{
				id: resultHover
				cursorShape: Qt.PointingHandCursor
			}

			TapHandler
			{
				onTapped:
					list.activated(resultRow.index, resultRow.modelData)
			}
		}

		BusyIndicator
		{
			anchors.horizontalCenter: parent.horizontalCenter
			y: 30
			running: list.busy
			visible: list.busy
		}

		// Empty: say why, and offer somewhere to start.
		Column
		{
			anchors.horizontalCenter: parent.horizontalCenter
			y: 24
			width: parent.width - 20
			spacing: 12
			visible: !list.busy && list.count === 0

			Text
			{
				width: parent.width
				text: list.message
				color: Qt.rgba(1, 1, 1, 0.6)
				font.pixelSize: 12
				wrapMode: Text.WordWrap
				horizontalAlignment: Text.AlignHCenter
			}

			Flow
			{
				width: parent.width
				spacing: 6

				Repeater
				{
					model: list.suggestions

					delegate: Chip
					{
						required property string modelData

						label: modelData

						onClicked:
							list.suggestionPicked(modelData)
					}
				}
			}
		}
	}
}
