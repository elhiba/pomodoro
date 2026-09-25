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

				Text
				{
					text: "TIMER"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					bottomPadding: 6
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
