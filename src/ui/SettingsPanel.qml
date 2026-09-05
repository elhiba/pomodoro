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

		width: Math.min(360, rootPanel.width * 0.9)
		height: rootPanel.height

		// Parked just past the right edge when closed.
		x: rootPanel.open ? rootPanel.width - width : rootPanel.width

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
