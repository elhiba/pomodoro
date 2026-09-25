// Delegates reach rootPanel from inside nested components.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import Pomodoro

// The music panel: a small window inside the app, opened from the title bar, for picking
// and controlling what plays without going through the settings.
//
// On top: the three sources as icons, then what is playing now -- cover, title, and
// previous / play-pause / next. Below, the chosen source's own view: the station list
// (with "+" to add one's own), YouTube search and playlists, or the Spotify library.
Item
{
	id: rootPanel

	required property var stations

	property bool open: false
	property color themeColor: "#12130F"

	// Skip on the radio list is a change of station, which only Main.qml can make.
	signal skipRequested(int step)

	// A checked link to keep on the radio list, and one of those to forget again.
	signal stationAdded(string name, string note, string url)
	signal stationRemoved(string url)

	// The built-in Spotify player's shelf: Liked Songs, the picks and the user's saved
	// links, as { name, uri, image, custom }. Picking plays; Main.qml writes the settings.
	required property var spotifyShelf

	signal spotifyPicked(string uri)
	signal spotifyAdded(string name, string uri, string image)
	signal spotifyRemoved(string uri)

	readonly property bool typing: youtubeSearch.activeFocus || spotifySearch.activeFocus
		|| stationLinkField.activeFocus || stationNameField.activeFocus
		|| clientIdField.activeFocus || spotifyLinkField.activeFocus || rootPanel.searchKeyTyping

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
					maximumLineCount: fromLine.visible ? 1 : 2
					wrapMode: Text.Wrap
				}

				// Which list the song belongs to, so it is clear what next will play.
				Text
				{
					id: fromLine

					width: parent.width
					visible: MusicPlayer.playingFrom.length > 0 && !MusicPlayer.failed

					text: "From " + MusicPlayer.playingFrom
					textFormat: Text.PlainText
					color: Qt.rgba(1, 1, 1, 0.45)
					font.pixelSize: 11
					elide: Text.ElideRight
				}
			}

			Row
			{
				id: controls

				anchors.right: parent.right
				anchors.rightMargin: 10
				anchors.verticalCenter: parent.verticalCenter

				spacing: 4

				// Like the song playing now. Only where Spotify lets us save it (a
				// developer key); hidden rather than offered and refused.
				TransportButton
				{
					visible: rootPanel.source === "spotify" && MusicPlayer.spotify.canSearch
						&& MusicPlayer.spotify.hasCurrentTrack
					iconSource: MusicPlayer.spotify.currentLiked ? "assets/icons/heartFilled.svg" : "assets/icons/heart.svg"
					iconColor: MusicPlayer.spotify.currentLiked ? "#1ed760" : "white"

					ToolTip.visible: hovered
					ToolTip.delay: 500
					ToolTip.text: MusicPlayer.spotify.currentLiked ? "Remove from Liked Songs" : "Add to Liked Songs"

					onClicked:
						MusicPlayer.spotify.toggleLike()
				}

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

		// "Added to the queue" and the like, over the bottom of the card.
		Rectangle
		{
			id: noticePill

			anchors.horizontalCenter: parent.horizontalCenter
			anchors.bottom: parent.bottom
			anchors.bottomMargin: 16
			z: 10

			width: noticeText.implicitWidth + 28
			height: 32
			radius: 16
			color: "#f2ffffff"
			opacity: 0
			visible: opacity > 0

			Behavior on opacity
			{
				NumberAnimation { duration: 200 }
			}

			Text
			{
				id: noticeText

				anchors.centerIn: parent
				color: "#333333"
				font.pixelSize: 12
				font.bold: true
			}
		}

		// ---------------------------------------------------------------- timeline

		// Where in the song playback is; drag or tap to jump.
		Item
		{
			id: timeline

			anchors.top: nowPlaying.bottom
			anchors.topMargin: MusicPlayer.seekable ? 8 : 0
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.leftMargin: 18
			anchors.rightMargin: 18

			// Songs and videos only: a radio station or a live stream has no length.
			visible: MusicPlayer.seekable
			height: MusicPlayer.seekable ? 24 : 0

			Text
			{
				id: elapsedText

				anchors.left: parent.left
				anchors.verticalCenter: parent.verticalCenter

				width: 38
				text: rootPanel.clock(seekBar.dragging
					? seekBar.shownValue * MusicPlayer.duration
					: MusicPlayer.position)
				color: Qt.rgba(1, 1, 1, 0.7)
				font.pixelSize: 11
				font.family: "JetBrains Mono"
			}

			SquigglySlider
			{
				id: seekBar

				anchors.left: elapsedText.right
				anchors.right: totalText.left
				anchors.leftMargin: 6
				anchors.rightMargin: 6
				anchors.verticalCenter: parent.verticalCenter

				value: MusicPlayer.duration > 0 ? MusicPlayer.position / MusicPlayer.duration : 0
				playing: MusicPlayer.status === MusicPlayer.Playing

				onSeekRequested: (fraction) => MusicPlayer.seek(Math.round(fraction * MusicPlayer.duration))
			}

			Text
			{
				id: totalText

				anchors.right: parent.right
				anchors.verticalCenter: parent.verticalCenter

				width: 38
				horizontalAlignment: Text.AlignRight
				text: rootPanel.clock(MusicPlayer.duration)
				color: Qt.rgba(1, 1, 1, 0.7)
				font.pixelSize: 11
				font.family: "JetBrains Mono"
			}
		}

		// ---------------------------------------------------------------- volume

		// The music's volume and whether it plays only while focusing -- the two things
		// changed most often, kept next to the music rather than in the settings.
		Item
		{
			id: mixer

			anchors.top: timeline.bottom
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

				// A remote-controlled Spotify has its own volume; this one would change
				// nothing there. The built-in player follows it.
				visible: rootPanel.source !== "spotify" || MusicPlayer.spotify.builtInPlayer

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
				visible: rootPanel.source === "radio" && !rootPanel.addingStation

				clip: true
				boundsBehavior: Flickable.StopAtBounds

				// A few pixels short of the full width so the scroll bar has its own lane.
				cellWidth: (width - 8) / 2
				cellHeight: 64

				// The last tile is "+", which opens the form to add a station.
				model: rootPanel.stations.concat([{ add: true }])

				ScrollBar.vertical: SlimScrollBar {}

				delegate: Item
				{
					id: stationCell

					required property var modelData
					required property int index

					readonly property bool adder: stationCell.modelData.add === true
					readonly property bool current: !stationCell.adder
						&& AppSettings.radioUrl === stationCell.modelData.url

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

						border.color: stationCell.current ? Qt.rgba(1, 1, 1, 0.6)
							: stationCell.adder ? Qt.rgba(1, 1, 1, 0.25) : "transparent"
						border.width: 1

						Row
						{
							anchors.centerIn: parent
							spacing: 8
							visible: stationCell.adder

							Image
							{
								anchors.verticalCenter: parent.verticalCenter
								width: 18
								height: 18
								source: "assets/icons/plus.svg"
								sourceSize.width: 36
								sourceSize.height: 36
							}

							Text
							{
								anchors.verticalCenter: parent.verticalCenter
								text: "Add a station"
								color: "white"
								font.pixelSize: 13
							}
						}

						Image
						{
							id: stationIcon

							visible: !stationCell.adder

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
							visible: !stationCell.adder

							anchors.left: stationIcon.right
							anchors.leftMargin: 10
							anchors.right: parent.right
							anchors.rightMargin: stationCell.modelData.custom ? 26 : 8
							anchors.verticalCenter: parent.verticalCenter

							Text
							{
								width: parent.width
								text: stationCell.modelData.name || ""
								color: "white"
								font.pixelSize: 13
								font.bold: stationCell.current
								elide: Text.ElideRight
							}

							Text
							{
								width: parent.width
								text: stationCell.modelData.note || ""
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
								if (stationCell.adder)
								{
									rootPanel.addingStation = true
									stationLinkField.forceActiveFocus()
									return
								}

								AppSettings.radioUrl = stationCell.modelData.url

								if (!MusicPlayer.active)
									MusicPlayer.play()
							}
						}

						// A station of one's own can be taken off the list again; the ones
						// the app ships with cannot.
						Button
						{
							id: removeStationBtn

							anchors.right: parent.right
							anchors.top: parent.top
							anchors.rightMargin: 4
							anchors.topMargin: 4

							width: 22
							height: 22

							visible: stationCell.modelData.custom === true
							opacity: stationHover.hovered || removeStationBtn.hovered ? 1.0 : 0.0

							background: Rectangle
							{
								radius: 11
								color: removeStationBtn.hovered ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.12)
							}

							icon.source: "assets/icons/close.svg"
							icon.color: "white"
							icon.width: 10
							icon.height: 10

							ToolTip.visible: removeStationBtn.hovered
							ToolTip.delay: 500
							ToolTip.text: "Remove this station"

							onClicked:
								rootPanel.stationRemoved(stationCell.modelData.url)
						}
					}
				}
			}

			// Adding a station: paste a link, and it is checked straight away. The station's
			// own name and description fill in; only a link that played audio can be saved.
			Column
			{
				id: addStationForm

				anchors.fill: parent
				spacing: 10
				visible: rootPanel.source === "radio" && rootPanel.addingStation

				// The name typed by hand wins over the one the station announces.
				property bool nameEdited: false

				StationProbe
				{
					id: stationProbe

					onChanged:
						if (stationProbe.state === StationProbe.Valid && !addStationForm.nameEdited)
							stationNameField.text = stationProbe.name
				}

				// Checked a moment after typing stops, so a link pasted in one go is
				// checked once, and one typed by hand is not checked at every letter.
				Timer
				{
					id: probeDelay
					interval: 600
					onTriggered: stationProbe.check(stationLinkField.text)
				}

				Text
				{
					width: parent.width
					text: "Paste the link of an internet radio stream. It is checked before it can be saved."
					color: Qt.rgba(1, 1, 1, 0.75)
					font.pixelSize: 12
					wrapMode: Text.WordWrap
				}

				SearchField
				{
					id: stationLinkField

					width: parent.width
					placeholderText: "https://…"
					iconSource: "assets/icons/radio.svg"

					onTextEdited: probeDelay.restart()
					onSubmitted: (value) =>
					{
						probeDelay.stop()
						stationProbe.check(value)
					}
				}

				// What the check found, or why the link was refused.
				Row
				{
					width: parent.width
					spacing: 8
					visible: stationProbe.state !== StationProbe.Empty

					Rectangle
					{
						anchors.verticalCenter: parent.verticalCenter
						width: 8
						height: 8
						radius: 4

						color: stationProbe.state === StationProbe.Valid ? "#7ee07e"
							: stationProbe.state === StationProbe.Invalid ? "#ff7b7b"
							: Qt.rgba(1, 1, 1, 0.6)

						SequentialAnimation on opacity
						{
							running: stationProbe.state === StationProbe.Checking
							loops: Animation.Infinite
							alwaysRunToEnd: true

							NumberAnimation { to: 0.2; duration: 500 }
							NumberAnimation { to: 1.0; duration: 500 }
						}
					}

					Text
					{
						width: parent.width - 16
						text: stationProbe.state === StationProbe.Checking ? "Checking the link…"
							: stationProbe.state === StationProbe.Valid
								? "It plays" + (stationProbe.note.length > 0 ? " · " + stationProbe.note : "")
							: stationProbe.message
						textFormat: Text.PlainText
						color: stationProbe.state === StationProbe.Invalid ? "#ffb3b3" : Qt.rgba(1, 1, 1, 0.75)
						font.pixelSize: 12
						wrapMode: Text.WordWrap
					}
				}

				SearchField
				{
					id: stationNameField

					width: parent.width
					visible: stationProbe.state === StationProbe.Valid
					placeholderText: "Station name"
					iconSource: "assets/icons/musicNote.svg"

					onTextEdited: addStationForm.nameEdited = true
					onSubmitted: saveStationBtn.clicked()
				}

				Row
				{
					anchors.right: parent.right
					spacing: 8

					Chip
					{
						label: "Cancel"
						onClicked: rootPanel.closeStationForm()
					}

					Chip
					{
						id: saveStationBtn

						readonly property bool alreadyThere: rootPanel.stations.some(
							(station) => station.url === stationProbe.url)

						label: saveStationBtn.alreadyThere ? "Already on the list" : "Save station"
						selected: saveStationBtn.enabled
						enabled: stationProbe.state === StationProbe.Valid
							&& stationNameField.text.trim().length > 0
							&& !saveStationBtn.alreadyThere
						opacity: saveStationBtn.enabled ? 1.0 : 0.5

						onClicked:
						{
							if (!saveStationBtn.enabled)
								return

							rootPanel.stationAdded(stationNameField.text.trim(), stationProbe.note,
								stationProbe.url)
							rootPanel.closeStationForm()
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
						text: MusicPlayer.spotify.builtInPlayer
							? "Sign in with your Spotify account to search your music and play it right here in Pomodoro. Spotify Premium is needed."
							: MusicPlayer.spotify.clientId.length > 0
							? "Sign in with your Spotify account to search your music and control it from here. It plays in your Spotify app, with Premium."
							: "Spotify sign-in is not switched on in this copy of Pomodoro yet."
						color: "white"
						font.pixelSize: 13
						wrapMode: Text.WordWrap
						horizontalAlignment: Text.AlignHCenter
					}

					Text
					{
						width: parent.width
						visible: MusicPlayer.spotify.statusText.length > 0 && !MusicPlayer.spotify.connecting
							&& (MusicPlayer.spotify.clientId.length > 0 || MusicPlayer.spotify.builtInPlayer)
						text: MusicPlayer.spotify.statusText
						textFormat: Text.PlainText
						color: Qt.rgba(1, 1, 1, 0.6)
						font.pixelSize: 11
						wrapMode: Text.WordWrap
						horizontalAlignment: Text.AlignHCenter
					}

					// A build without a Client ID of its own keeps the developer set-up out of
					// sight: a listener should never be asked about redirect URIs and ports.
					// Whoever does have a key of their own opens it here.
					Chip
					{
						anchors.horizontalCenter: parent.horizontalCenter
						visible: !MusicPlayer.spotify.builtInPlayer
							&& MusicPlayer.spotify.builtInClientId.length === 0 && !rootPanel.spotifyDeveloper
						label: "I have a Spotify developer key"

						onClicked:
							rootPanel.spotifyDeveloper = true
					}

					SearchField
					{
						id: clientIdField

						width: parent.width
						visible: !MusicPlayer.spotify.builtInPlayer
							&& MusicPlayer.spotify.builtInClientId.length === 0 && rootPanel.spotifyDeveloper
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
						visible: MusicPlayer.spotify.clientId.length > 0 || MusicPlayer.spotify.builtInPlayer
						// The built-in player keeps its sign-in page waiting, so a closed browser
						// tab can be opened again from here.
						label: !MusicPlayer.spotify.connecting ? "Sign in with Spotify"
							: MusicPlayer.spotify.builtInPlayer ? "Open the sign-in page again"
							: "Waiting for the browser…"
						enabled: !MusicPlayer.spotify.connecting || MusicPlayer.spotify.builtInPlayer

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
						visible: MusicPlayer.spotify.canSearch
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
						visible: MusicPlayer.spotify.canSearch

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

					visible: MusicPlayer.spotify.connected && MusicPlayer.spotify.canSearch
						&& MusicPlayer.spotify.openedUri.length === 0
					model: MusicPlayer.spotify.results
					busy: MusicPlayer.spotify.searching
					message: MusicPlayer.spotify.resultsError.length > 0
						? MusicPlayer.spotify.resultsError
						: "Search, or pick one of your lists above."

					// A playlist or album opens to its songs when the built-in player can
					// list them; a song plays.
					queueable: true
					onQueueRequested: (item) => MusicPlayer.spotify.addToQueue(item.uri)

					onActivated: (index, item) =>
					{
						if (item.kind !== "track" && MusicPlayer.spotify.builtInPlayer)
							MusicPlayer.spotify.openContext(item.uri, item.title)
						else
							MusicPlayer.playSpotifyResult(index)
					}
				}

				// An opened playlist, album or Liked Songs: its songs, with back and play all.
				Item
				{
					id: spotifyOpened

					anchors.top: spotifyTop.bottom
					anchors.topMargin: 8
					anchors.bottom: parent.bottom
					anchors.left: parent.left
					anchors.right: parent.right

					visible: MusicPlayer.spotify.connected && MusicPlayer.spotify.openedUri.length > 0

					Item
					{
						id: openedHeader

						anchors.top: parent.top
						anchors.left: parent.left
						anchors.right: parent.right
						height: 36

						Chip
						{
							id: openedBack

							anchors.left: parent.left
							anchors.verticalCenter: parent.verticalCenter
							label: "‹ Back"

							onClicked: MusicPlayer.spotify.closeContext()
						}

						Text
						{
							anchors.left: openedBack.right
							anchors.leftMargin: 10
							anchors.right: playAllBtn.left
							anchors.rightMargin: 10
							anchors.verticalCenter: parent.verticalCenter

							text: MusicPlayer.spotify.openedTitle
							textFormat: Text.PlainText
							color: "white"
							font.pixelSize: 14
							font.bold: true
							elide: Text.ElideRight
						}

						PillButton
						{
							id: playAllBtn

							anchors.right: parent.right
							anchors.verticalCenter: parent.verticalCenter
							label: "Play all"

							onClicked: rootPanel.spotifyPicked(MusicPlayer.spotify.openedUri)
						}
					}

					ResultList
					{
						anchors.top: openedHeader.bottom
						anchors.topMargin: 6
						anchors.bottom: parent.bottom
						anchors.left: parent.left
						anchors.right: parent.right

						model: MusicPlayer.spotify.results
						busy: MusicPlayer.spotify.searching && MusicPlayer.spotify.results.length === 0
						message: MusicPlayer.spotify.resultsError.length > 0
							? MusicPlayer.spotify.resultsError
							: "Reading the songs…"

						queueable: true
						onQueueRequested: (item) => MusicPlayer.spotify.addToQueue(item.uri)

						onActivated: (index, item) => MusicPlayer.playSpotifyResult(index)
					}
				}

				// The built-in player without search: a shelf of playlists to tap, links the
				// user pastes, and -- tucked at the bottom -- search for those who add a
				// developer key of their own.
				GridView
				{
					id: spotifyShelfGrid

					anchors.top: spotifyTop.bottom
					anchors.topMargin: 8
					anchors.bottom: parent.bottom
					anchors.left: parent.left
					anchors.right: parent.right

					visible: MusicPlayer.spotify.connected && !MusicPlayer.spotify.canSearch
						&& !rootPanel.addingSpotifyLink && MusicPlayer.spotify.openedUri.length === 0

					clip: true
					boundsBehavior: Flickable.StopAtBounds
					cellWidth: (width - 8) / 2
					cellHeight: 64

					model: rootPanel.spotifyShelf.concat([{ add: true }])

					ScrollBar.vertical: SlimScrollBar {}

					delegate: Item
					{
						id: shelfCell

						required property var modelData

						readonly property bool adder: shelfCell.modelData.add === true
						readonly property bool current: !shelfCell.adder
							&& AppSettings.spotifyUri === shelfCell.modelData.uri

						width: spotifyShelfGrid.cellWidth
						height: spotifyShelfGrid.cellHeight

						Rectangle
						{
							anchors.fill: parent
							anchors.margins: 3
							radius: 10

							color: shelfCell.current
								? Qt.rgba(1, 1, 1, 0.24)
								: shelfHover.hovered ? Qt.rgba(1, 1, 1, 0.12) : Qt.rgba(1, 1, 1, 0.05)

							border.color: shelfCell.current ? Qt.rgba(1, 1, 1, 0.6)
								: shelfCell.adder ? Qt.rgba(1, 1, 1, 0.25) : "transparent"
							border.width: 1

							Row
							{
								anchors.centerIn: parent
								spacing: 8
								visible: shelfCell.adder

								Image
								{
									anchors.verticalCenter: parent.verticalCenter
									width: 18
									height: 18
									source: "assets/icons/plus.svg"
									sourceSize.width: 36
									sourceSize.height: 36
								}

								Text
								{
									anchors.verticalCenter: parent.verticalCenter
									text: "Add a Spotify link"
									color: "white"
									font.pixelSize: 13
								}
							}

							// The cover, or the Spotify logo for Liked Songs.
							Rectangle
							{
								id: shelfArt

								anchors.left: parent.left
								anchors.leftMargin: 7
								anchors.verticalCenter: parent.verticalCenter

								width: 44
								height: 44
								radius: 6
								clip: true
								visible: !shelfCell.adder
								color: Qt.rgba(0, 0, 0, 0.2)

								Image
								{
									anchors.fill: parent
									anchors.margins: (shelfCell.modelData.image || "").length > 0 ? 0 : 10
									source: (shelfCell.modelData.image || "").length > 0
										? shelfCell.modelData.image
										: "assets/icons/spotify.svg"
									sourceSize.width: 88
									sourceSize.height: 88
									fillMode: Image.PreserveAspectCrop
									asynchronous: true
								}
							}

							Text
							{
								anchors.left: shelfArt.right
								anchors.leftMargin: 10
								anchors.right: parent.right
								anchors.rightMargin: shelfCell.modelData.custom ? 26 : 8
								anchors.verticalCenter: parent.verticalCenter

								visible: !shelfCell.adder
								text: shelfCell.modelData.name || ""
								textFormat: Text.PlainText
								color: "white"
								font.pixelSize: 13
								font.bold: shelfCell.current
								wrapMode: Text.WordWrap
								maximumLineCount: 2
								elide: Text.ElideRight
							}

							HoverHandler
							{
								id: shelfHover
								cursorShape: Qt.PointingHandCursor
							}

							TapHandler
							{
								onTapped:
								{
									if (shelfCell.adder)
									{
										rootPanel.addingSpotifyLink = true
										spotifyLinkField.forceActiveFocus()
										return
									}

									// Opened first, so the songs can be seen and one picked;
									// "Play all" is one tap away at the top of the list.
									MusicPlayer.spotify.openContext(shelfCell.modelData.uri, shelfCell.modelData.name)
								}
							}

							Button
							{
								id: removeShelfBtn

								anchors.right: parent.right
								anchors.top: parent.top
								anchors.rightMargin: 4
								anchors.topMargin: 4

								width: 22
								height: 22

								visible: shelfCell.modelData.custom === true
								opacity: shelfHover.hovered || removeShelfBtn.hovered ? 1.0 : 0.0

								background: Rectangle
								{
									radius: 11
									color: removeShelfBtn.hovered ? Qt.rgba(1, 1, 1, 0.3) : Qt.rgba(1, 1, 1, 0.12)
								}

								icon.source: "assets/icons/close.svg"
								icon.color: "white"
								icon.width: 10
								icon.height: 10

								ToolTip.visible: removeShelfBtn.hovered
								ToolTip.delay: 500
								ToolTip.text: "Remove from the shelf"

								onClicked:
									rootPanel.spotifyRemoved(shelfCell.modelData.uri)
							}
						}
					}

					// Search is there for whoever brings a developer key; nobody else needs
					// to know what one is.
					footer: Column
					{
						width: spotifyShelfGrid.width - 8
						topPadding: 10
						spacing: 8

						Chip
						{
							anchors.horizontalCenter: parent.horizontalCenter
							visible: !rootPanel.searchKeyOpen
							label: "Want search? Add your Spotify developer key"

							onClicked:
								rootPanel.searchKeyOpen = true
						}

						Text
						{
							width: parent.width
							visible: rootPanel.searchKeyOpen
							text: "Spotify only allows search through a free developer key of your own. It takes about two minutes; the music keeps playing here."
							color: Qt.rgba(1, 1, 1, 0.75)
							font.pixelSize: 12
							wrapMode: Text.WordWrap
						}

						// The steps, each with a button for the part that can be clicked or
						// copied, so nothing has to be typed out by hand.
						Repeater
						{
							model: rootPanel.searchKeyOpen ? [
								{ text: "1. Open Spotify's developer page and log in with your Spotify account.",
									button: "Open", action: "open" },
								{ text: "2. Create an app. Name it Pomodoro; any description will do.",
									button: "Copy name", action: "name" },
								{ text: "3. Under Redirect URIs, paste this and press Add: " + MusicPlayer.spotify.redirectUri,
									button: "Copy", action: "redirect" },
								{ text: "4. Tick Web API, accept the terms and Save.", button: "", action: "" },
								{ text: "5. Open the app's Settings, copy its Client ID, paste it below and press Enter.",
									button: "", action: "" }
							] : []

							delegate: Item
							{
								id: stepRow

								required property var modelData

								width: parent.width
								height: Math.max(stepText.implicitHeight, 28)

								Text
								{
									id: stepText

									anchors.left: parent.left
									anchors.right: stepButton.visible ? stepButton.left : parent.right
									anchors.rightMargin: 8
									anchors.verticalCenter: parent.verticalCenter

									text: stepRow.modelData.text
									textFormat: Text.PlainText
									color: "white"
									font.pixelSize: 12
									wrapMode: Text.WrapAnywhere
								}

								Chip
								{
									id: stepButton

									anchors.right: parent.right
									anchors.verticalCenter: parent.verticalCenter

									visible: stepRow.modelData.button.length > 0
									label: stepRow.modelData.button

									onClicked:
									{
										switch (stepRow.modelData.action)
										{
											case "open":
												Qt.openUrlExternally("https://developer.spotify.com/dashboard/create")
												break
											case "name":
												rootPanel.copy("Pomodoro")
												stepButton.label = "Copied"
												break
											case "redirect":
												rootPanel.copy(MusicPlayer.spotify.redirectUri)
												stepButton.label = "Copied"
												break
										}
									}
								}
							}
						}

						SearchField
						{
							id: searchKeyField

							width: parent.width
							visible: rootPanel.searchKeyOpen
							text: AppSettings.spotifyClientId
							placeholderText: "Spotify Client ID, then Enter"
							iconSource: "assets/icons/spotify.svg"

							onSubmitted: (value) => AppSettings.spotifyClientId = value
							onActiveFocusChanged: rootPanel.searchKeyTyping = activeFocus
						}

						PillButton
						{
							anchors.horizontalCenter: parent.horizontalCenter
							visible: rootPanel.searchKeyOpen && MusicPlayer.spotify.clientId.length > 0
							label: MusicPlayer.spotify.connecting ? "Waiting for the browser…" : "Sign in for search"
							enabled: !MusicPlayer.spotify.connecting

							onClicked:
								MusicPlayer.spotify.connectSearch()
						}
					}
				}

				// Adding a link: pasted, named by Spotify, saved to the shelf and played.
				Column
				{
					id: addSpotifyForm

					anchors.top: spotifyTop.bottom
					anchors.topMargin: 8
					anchors.left: parent.left
					anchors.right: parent.right

					spacing: 10
					visible: MusicPlayer.spotify.connected && !MusicPlayer.spotify.canSearch
						&& rootPanel.addingSpotifyLink

					property string foundUri: ""
					property string foundTitle: ""
					property string foundImage: ""
					property string lookupError: ""
					property bool looking: false

					Connections
					{
						target: MusicPlayer.spotify

						function onLinkLookedUp(uri, title, image, error)
						{
							addSpotifyForm.looking = false
							addSpotifyForm.foundUri = uri
							addSpotifyForm.foundTitle = title
							addSpotifyForm.foundImage = image
							addSpotifyForm.lookupError = error
						}
					}

					Timer
					{
						id: spotifyLookupDelay
						interval: 500
						onTriggered: rootPanel.lookUpSpotifyLink(spotifyLinkField.text)
					}

					Text
					{
						width: parent.width
						text: "In Spotify, open a playlist, album or song, choose Share, then Copy link, and paste it here."
						color: Qt.rgba(1, 1, 1, 0.75)
						font.pixelSize: 12
						wrapMode: Text.WordWrap
					}

					SearchField
					{
						id: spotifyLinkField

						width: parent.width
						placeholderText: "https://open.spotify.com/…"
						iconSource: "assets/icons/spotify.svg"

						onTextEdited: spotifyLookupDelay.restart()
						onSubmitted: (value) =>
						{
							spotifyLookupDelay.stop()
							rootPanel.lookUpSpotifyLink(value)
						}
					}

					Text
					{
						width: parent.width
						visible: addSpotifyForm.looking || addSpotifyForm.lookupError.length > 0
						text: addSpotifyForm.looking ? "Looking it up…" : addSpotifyForm.lookupError
						textFormat: Text.PlainText
						color: addSpotifyForm.lookupError.length > 0 ? "#ffb3b3" : Qt.rgba(1, 1, 1, 0.75)
						font.pixelSize: 12
						wrapMode: Text.WordWrap
					}

					// What was found, so it is clear what will be saved.
					Row
					{
						width: parent.width
						spacing: 10
						visible: addSpotifyForm.foundUri.length > 0

						Image
						{
							width: 48
							height: 48
							source: addSpotifyForm.foundImage
							sourceSize.width: 96
							sourceSize.height: 96
							fillMode: Image.PreserveAspectCrop
						}

						Text
						{
							anchors.verticalCenter: parent.verticalCenter
							width: parent.width - 58
							text: addSpotifyForm.foundTitle
							textFormat: Text.PlainText
							color: "white"
							font.pixelSize: 13
							font.bold: true
							wrapMode: Text.WordWrap
						}
					}

					Row
					{
						anchors.right: parent.right
						spacing: 8

						Chip
						{
							label: "Cancel"
							onClicked: rootPanel.closeSpotifyForm()
						}

						Chip
						{
							id: saveSpotifyBtn

							label: "Save and play"
							selected: saveSpotifyBtn.enabled
							enabled: addSpotifyForm.foundUri.length > 0
							opacity: saveSpotifyBtn.enabled ? 1.0 : 0.5

							onClicked:
							{
								if (!saveSpotifyBtn.enabled)
									return

								rootPanel.spotifyAdded(addSpotifyForm.foundTitle, addSpotifyForm.foundUri,
									addSpotifyForm.foundImage)
								rootPanel.closeSpotifyForm()
							}
						}
					}
				}
			}

		}
	}

	// ---------------------------------------------------------------- state and helpers

	// Shows the Client ID field, for a build that carries none.
	property bool spotifyDeveloper: AppSettings.spotifyClientId.length > 0

	// True while the "add a station" form covers the station list.
	property bool addingStation: false

	// The same for the Spotify shelf's "add a link" form, and whether the developer-key
	// section under the shelf is open.
	property bool addingSpotifyLink: false
	property bool searchKeyOpen: false

	// The Client ID field lives in the shelf's footer, whose ids are out of reach here.
	property bool searchKeyTyping: false

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
			default: return "assets/icons/radio.svg"
		}
	}

	// 83000 ms -> "1:23"; an hour or more -> "1:02:03".
	function clock(milliseconds)
	{
		let total = Math.max(0, Math.floor(milliseconds / 1000))
		let hours = Math.floor(total / 3600)
		let minutes = Math.floor((total % 3600) / 60)
		let seconds = total % 60
		let padded = (n) => n < 10 ? "0" + n : "" + n

		return hours > 0
			? hours + ":" + padded(minutes) + ":" + padded(seconds)
			: minutes + ":" + padded(seconds)
	}

	// Puts text on the clipboard, for the developer-key steps.
	function copy(text)
	{
		copyHelper.text = text
		copyHelper.selectAll()
		copyHelper.copy()
		copyHelper.deselect()
	}

	TextEdit
	{
		id: copyHelper
		visible: false
	}

	Connections
	{
		target: MusicPlayer.spotify

		function onNotice(text)
		{
			noticeText.text = text
			noticePill.opacity = 1
			noticeTimer.restart()
		}
	}

	Timer
	{
		id: noticeTimer
		interval: 2200
		onTriggered: noticePill.opacity = 0
	}

	function lookUpSpotifyLink(text)
	{
		addSpotifyForm.foundUri = ""
		addSpotifyForm.lookupError = ""
		addSpotifyForm.looking = text.trim().length > 0

		if (addSpotifyForm.looking)
			MusicPlayer.spotify.lookUpLink(text)
	}

	function closeSpotifyForm()
	{
		spotifyLookupDelay.stop()
		spotifyLinkField.text = ""
		addSpotifyForm.foundUri = ""
		addSpotifyForm.foundTitle = ""
		addSpotifyForm.foundImage = ""
		addSpotifyForm.lookupError = ""
		addSpotifyForm.looking = false
		rootPanel.addingSpotifyLink = false
	}

	function closeStationForm()
	{
		probeDelay.stop()
		stationProbe.reset()
		stationLinkField.text = ""
		stationNameField.text = ""
		addStationForm.nameEdited = false
		rootPanel.addingStation = false
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
			&& MusicPlayer.spotify.canSearch
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

		// Centred on the row: previous and next are smaller than play, and a Row lines
		// its items up along the top.
		anchors.verticalCenter: parent ? parent.verticalCenter : undefined

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

		// Songs get an "add to queue" button (Spotify lists).
		property bool queueable: false

		signal activated(int index, var item)
		signal suggestionPicked(string text)
		signal queueRequested(var item)

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
				anchors.rightMargin: resultRow.collection ? 26 : queueBtn.visible ? 44 : 8
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

			// Plays after the current song rather than instead of it. Faint until the row
			// is hovered, so a long list does not turn into a column of buttons.
			Button
			{
				id: queueBtn

				anchors.right: parent.right
				anchors.rightMargin: 6
				anchors.verticalCenter: parent.verticalCenter

				width: 32
				height: 32

				visible: list.queueable && resultRow.modelData.kind === "track"
				opacity: resultHover.hovered || queueBtn.hovered ? 1.0 : 0.35

				background: Rectangle
				{
					radius: 16
					color: queueBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : "transparent"
				}

				icon.source: "assets/icons/queueAdd.svg"
				icon.color: "white"
				icon.width: 18
				icon.height: 18

				ToolTip.visible: queueBtn.hovered
				ToolTip.delay: 500
				ToolTip.text: "Play next"

				onClicked:
					list.queueRequested(resultRow.modelData)
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
