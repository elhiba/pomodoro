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
			anchors.topMargin: 20
			anchors.bottomMargin: 20
			anchors.leftMargin: 20
			anchors.rightMargin: 4

			// The text keeps its 20 pixel margin; the bar lives in the gap beside it
			// instead of on top of it.
			rightPadding: 16

			clip: true
			contentWidth: availableWidth
			ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
			ScrollBar.vertical: SlimScrollBar
			{
				parent: body
				x: body.width - width
				y: body.topPadding
				height: body.availableHeight
			}

			Column
			{
				width: body.availableWidth
				spacing: 4

				SectionHeader
				{
					id: sectionTimer

					title: "TIMER"
					expanded: true
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionTimer.expanded

					NumberSetting
					{
						label: "Rounds before long break"
						value: AppSettings.roundsBeforeLongBreak
						minimum: AppSettings.minimumRounds
						maximum: AppSettings.maximumRounds
						suffix: ""

						onValueModified: (newValue) => AppSettings.roundsBeforeLongBreak = newValue
					}
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				SectionHeader
				{
					id: sectionAppearance

					title: "APPEARANCE"
					expanded: false
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionAppearance.expanded

					// Colours for each kind of session.
					SectionHeader
					{
						id: coloursGroup

						title: "Colours"
						small: false
						summary: AppSettings.focusColor.length + AppSettings.shortBreakColor.length
							+ AppSettings.longBreakColor.length > 0 ? "Custom" : "Default"
					}

					Column
					{
						width: parent.width
						visible: coloursGroup.expanded

						ColorRow
						{
							width: parent.width
							label: "Focus"
							value: AppSettings.focusColor
							defaultColor: "#ba4949"

							onPicked: (colour) => AppSettings.focusColor = colour
						}

						ColorRow
						{
							width: parent.width
							label: "Short break"
							value: AppSettings.shortBreakColor
							defaultColor: "#38858a"

							onPicked: (colour) => AppSettings.shortBreakColor = colour
						}

						ColorRow
						{
							width: parent.width
							label: "Long break"
							value: AppSettings.longBreakColor
							defaultColor: "#397097"

							onPicked: (colour) => AppSettings.longBreakColor = colour
						}
					}

					// How the digits change; the timer behind the drawer shows it straight away.
					SectionHeader
					{
						id: animationGroup

						readonly property var styles: [
							{ key: "", label: "Still" },
							{ key: "roll", label: "Rolling" },
							{ key: "flip", label: "Flip" },
							{ key: "soft", label: "Soft" }
						]

						title: "Timer animation"
						small: false
						summary: animationGroup.styles.find((style) => style.key === AppSettings.timerStyle)?.label ?? "Still"
					}

					Flow
					{
						width: parent.width
						spacing: 6
						bottomPadding: 10
						visible: animationGroup.expanded

						Repeater
						{
							model: animationGroup.styles

							delegate: OptionChip
							{
								required property var modelData

								label: modelData.label
								selected: AppSettings.timerStyle === modelData.key

								onClicked: AppSettings.timerStyle = modelData.key
							}
						}
					}

					// The font for the whole app, timer included.
					SectionHeader
					{
						id: fontGroup

						title: "Font"
						small: false
						summary: AppSettings.appFont.length > 0 ? AppSettings.appFont : "Default"
					}

					Column
					{
						width: parent.width
						spacing: 8
						bottomPadding: 10
						visible: fontGroup.expanded

						TextField
						{
							id: fontSearch

							width: parent.width
							height: 36
							leftPadding: 12
							rightPadding: 12

							placeholderText: "Search fonts"
							placeholderTextColor: Qt.rgba(1, 1, 1, 0.45)
							color: "white"
							font.pixelSize: 14
							selectByMouse: true

							background: Rectangle
							{
								radius: 10
								color: Qt.rgba(0, 0, 0, 0.2)
								border.color: fontSearch.activeFocus ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.18)
							}
						}

						// Every font on the computer, each written in itself; installing a new
						// one in the system makes it appear here. "Default" is the system font,
						// with JetBrains Mono for the timer.
						Rectangle
						{
							width: parent.width
							height: 250
							radius: 10
							color: Qt.rgba(0, 0, 0, 0.18)
							clip: true

							ListView
							{
								id: fontList

								anchors.fill: parent
								anchors.margins: 4

								boundsBehavior: Flickable.StopAtBounds
								spacing: 2

								model:
								{
									let wanted = fontSearch.text.trim().toLowerCase()

									// "@" names are the vertical variants of CJK fonts: not for us.
									let families = Qt.fontFamilies().filter((family) =>
										!family.startsWith("@") && family.toLowerCase().includes(wanted))

									return wanted.length > 0 ? families : [""].concat(families)
								}

								ScrollBar.vertical: SlimScrollBar {}

								// The wheel over the list scrolls the list and nothing else: left to itself it also
								// reached the settings page behind it, and both moved.
								MouseArea
								{
									anchors.fill: parent
									z: 1
									acceptedButtons: Qt.NoButton

									onWheel: (wheel) =>
									{
										let step = wheel.pixelDelta.y !== 0 ? wheel.pixelDelta.y : wheel.angleDelta.y / 120 * 76
										let bottom = Math.max(0, fontList.contentHeight - fontList.height)

										fontList.contentY = Math.max(0, Math.min(bottom, fontList.contentY - step))
										wheel.accepted = true
									}
								}

								delegate: Rectangle
								{
									id: fontRow

									required property var modelData

									readonly property bool chosen: AppSettings.appFont === fontRow.modelData

									width: fontList.width - 8
									height: 38
									radius: 8

									color: fontRow.chosen
										? Qt.rgba(1, 1, 1, 0.22)
										: fontRowHover.hovered ? Qt.rgba(1, 1, 1, 0.1) : "transparent"

									Text
									{
										anchors.left: parent.left
										anchors.leftMargin: 12
										anchors.right: fontCheck.left
										anchors.rightMargin: 8
										anchors.verticalCenter: parent.verticalCenter

										// Shown in itself; its own font, so AppFont leaves it be.
										property bool ownFont: true

										text: fontRow.modelData.length > 0 ? fontRow.modelData : "Default"
										font.family: fontRow.modelData.length > 0 ? fontRow.modelData : font.family
										font.pixelSize: 16
										color: "white"
										elide: Text.ElideRight
									}

									Text
									{
										id: fontCheck

										anchors.right: parent.right
										anchors.rightMargin: 12
										anchors.verticalCenter: parent.verticalCenter

										visible: fontRow.chosen
										text: "✓"
										color: "white"
										font.pixelSize: 16
										font.bold: true
									}

									HoverHandler
									{
										id: fontRowHover
										cursorShape: Qt.PointingHandCursor
									}

									TapHandler
									{
										onTapped: AppSettings.appFont = fontRow.modelData
									}
								}
							}
						}

						ToggleSetting
						{
							label: "Use it for the Pomodoro title"
							checked: AppSettings.fontOnTitle
							accentColor: rootPanel.themeColor

							onToggleRequested: (wanted) => AppSettings.fontOnTitle = wanted
						}
					}
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				SectionHeader
				{
					id: sectionAutomation

					title: "AUTOMATION"
					expanded: false
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionAutomation.expanded

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
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				SectionHeader
				{
					id: sectionTasks

					title: "TASKS"
					expanded: false
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionTasks.expanded

					ToggleSetting
					{
						label: "Show the task list"
						checked: AppSettings.tasksEnabled
						accentColor: rootPanel.themeColor

						onToggleRequested: (wanted) => AppSettings.tasksEnabled = wanted
					}
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				SectionHeader
				{
					id: sectionSound

					title: "SOUND"
					expanded: false
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionSound.expanded

					SliderSetting
					{
						label: "Alarm volume"
						value: AppSettings.alarmVolume

						onValueModified: (newValue) => AppSettings.alarmVolume = newValue
						onPreviewRequested: SoundPlayer.playAlarm()
					}
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				SectionHeader
				{
					id: sectionWindow

					title: "WINDOW"
					expanded: false
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionWindow.expanded

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
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				// Discord: one switch. Nothing to sign in to -- the Discord app on this computer
				// is found on its own; the row says whether it was.
				SectionHeader
				{
					id: sectionDiscord

					title: "DISCORD"
					visible: DiscordPresence.available
					summary: !DiscordPresence.enabled ? "Off" : DiscordPresence.connected ? "Connected" : "Waiting for Discord"
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionDiscord.visible && sectionDiscord.expanded

					Rectangle
					{
						width: parent.width
						height: 52
						radius: 12
						color: Qt.rgba(0, 0, 0, 0.18)

						Image
						{
							id: discordLogo

							anchors.left: parent.left
							anchors.leftMargin: 14
							anchors.verticalCenter: parent.verticalCenter

							width: 26
							height: 26
							source: "assets/icons/discord.svg"
							sourceSize.width: 52
							sourceSize.height: 52
							opacity: DiscordPresence.connected ? 1.0 : 0.5
						}

						// Green when Discord is showing the activity, grey otherwise.
						Rectangle
						{
							anchors.right: discordLogo.right
							anchors.bottom: discordLogo.bottom
							anchors.rightMargin: -4
							anchors.bottomMargin: -4

							width: 11
							height: 11
							radius: 5.5
							color: DiscordPresence.connected ? "#3ba55d" : "#80848e"
							border.color: "#2b2d31"
							border.width: 2
						}

						Text
						{
							anchors.left: discordLogo.right
							anchors.leftMargin: 14
							anchors.right: parent.right
							anchors.rightMargin: 14
							anchors.verticalCenter: parent.verticalCenter

							text: DiscordPresence.statusText
							textFormat: Text.PlainText
							color: DiscordPresence.connected ? "white" : Qt.rgba(1, 1, 1, 0.7)
							font.pixelSize: 14
							elide: Text.ElideRight
						}
					}

					ToggleSetting
					{
						label: "Show my activity on Discord"
						checked: DiscordPresence.enabled
						accentColor: rootPanel.themeColor

						onToggleRequested: (wanted) => DiscordPresence.enabled = wanted
					}

					ToggleSetting
					{
						label: "Show the music I'm listening to"
						checked: AppSettings.discordShowMusic
						accentColor: rootPanel.themeColor
						opacity: DiscordPresence.enabled ? 1.0 : 0.5

						onToggleRequested: (wanted) => AppSettings.discordShowMusic = wanted
					}

					Text
					{
						width: parent.width
						bottomPadding: 8
						text: "Your profile shows “Playing Pomodoro” with what the timer is doing, while the Discord app is open on this computer."
						color: Qt.rgba(1, 1, 1, 0.55)
						font.pixelSize: 12
						wrapMode: Text.WordWrap
					}
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
					visible: DiscordPresence.available
				}

				SectionHeader
				{
					id: sectionAbout

					title: "ABOUT"
					expanded: false
				}

				Column
				{
					width: parent.width
					spacing: 4
					visible: sectionAbout.expanded

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

	// A chip for picking one of a few options.
	component OptionChip: Button
	{
		id: optionChip

		property string label: ""
		property bool selected: false

		height: 30
		leftPadding: 14
		rightPadding: 14

		background: Rectangle
		{
			radius: height / 2
			color: optionChip.selected
				? Qt.rgba(1, 1, 1, 0.9)
				: optionChip.hovered ? Qt.rgba(1, 1, 1, 0.18) : Qt.rgba(1, 1, 1, 0.08)
		}

		contentItem: Text
		{
			text: optionChip.label
			color: optionChip.selected ? "#333333" : "white"
			font.pixelSize: 13
			font.bold: optionChip.selected
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
	}

	// One session kind's colour: its name, a row of swatches -- the built-in colour
	// first -- and a box for any colour as #rrggbb. Picking the built-in one stores ""
	// so a later change of default reaches it.
	component ColorRow: Column
	{
		id: colorRow

		property string label: ""
		property string value: ""
		property string defaultColor: "#000000"

		signal picked(string colour)

		readonly property string shown: colorRow.value.length > 0 ? colorRow.value : colorRow.defaultColor
		readonly property var swatches: [
			"#ba4949", "#38858a", "#397097", "#7d5ba6", "#c0632f", "#4f7a4a",
			"#b0476d", "#2f6f8f", "#8a6d3b", "#46505a", "#1f2a44", "#a23b3b"
		]

		spacing: 6
		bottomPadding: 10

		Text
		{
			text: colorRow.label
			color: "white"
			font.pixelSize: 15
		}

		Flow
		{
			width: colorRow.width
			spacing: 6

			Repeater
			{
				// The default first, then the palette without it.
				model: [colorRow.defaultColor].concat(colorRow.swatches.filter((c) => c !== colorRow.defaultColor))

				delegate: Rectangle
				{
					id: swatch

					required property var modelData
					required property int index

					readonly property bool chosen: colorRow.shown.toLowerCase() === swatch.modelData.toLowerCase()

					width: 26
					height: 26
					radius: 13
					color: swatch.modelData

					border.color: "white"
					border.width: swatch.chosen ? 3 : swatchHover.hovered ? 1.5 : 0

					HoverHandler
					{
						id: swatchHover
						cursorShape: Qt.PointingHandCursor
					}

					TapHandler
					{
						onTapped: colorRow.picked(swatch.index === 0 ? "" : swatch.modelData)
					}

					ToolTip.visible: swatchHover.hovered && swatch.index === 0
					ToolTip.delay: 500
					ToolTip.text: "Default"
				}
			}

			TextField
			{
				id: hexField

				width: 86
				height: 26
				leftPadding: 8
				rightPadding: 8
				topPadding: 0
				bottomPadding: 0

				placeholderText: "#hex"
				placeholderTextColor: Qt.rgba(1, 1, 1, 0.4)
				color: "white"
				font.pixelSize: 12
				selectByMouse: true
				text: colorRow.value

				validator: RegularExpressionValidator { regularExpression: /#?[0-9a-fA-F]{0,6}/ }

				background: Rectangle
				{
					radius: 13
					color: Qt.rgba(0, 0, 0, 0.2)
					border.color: hexField.activeFocus ? Qt.rgba(1, 1, 1, 0.6) : Qt.rgba(1, 1, 1, 0.2)
				}

				onAccepted:
				{
					let hex = hexField.text.trim().replace("#", "")

					if (hex.length === 6)
						colorRow.picked("#" + hex.toLowerCase())
				}
			}
		}
	}
}
