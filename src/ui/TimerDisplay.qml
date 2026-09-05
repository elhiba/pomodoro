import QtQuick
import QtQuick.Controls

import Pomodoro

Item
{
    id: rootTimer

	// Sized and fed from Main.qml, so this component never reaches out to the window.
	required property PomodoroTimer timer
	property color themeColor: "#12130F"

	FontLoader
	{
        id: timerFont
        source: "assets/fonts/JetBrainsMono.ttf" 
    }

	Rectangle
	{
		id: glassPanel
		anchors.fill: parent
        
        // The Glass Recipe: 15% opaque white, smooth corners, 30% opaque border
        color: Qt.rgba(1, 1, 1, 0.15)
        radius: 24
        border.color: Qt.rgba(1, 1, 1, 0.3)
        border.width: 1

		Column
		{
			anchors.centerIn: parent
			spacing: 30

			Text
			{
				id: timeText
				text: rootTimer.timer.displayTime
				color: "white"
				font.family: timerFont.name
				font.pixelSize: glassPanel.width * 0.2

				horizontalAlignment: Text.AlignHCenter
				anchors.horizontalCenter: parent.horizontalCenter
			}

			Row
			{
				id: controls
				spacing: glassPanel.width * 0.05

				anchors.horizontalCenter: parent.horizontalCenter

				Button
				{
					id: startBtn
					width: glassPanel.width * 0.25
					height: glassPanel.height * 0.25

					property int depth: 6 

					background: Item
					{
						Rectangle
						{
							anchors.fill: parent
							anchors.topMargin: startBtn.depth 
							color: "#e0e0e0"
							radius: 8
						}

						Rectangle
						{
							width: parent.width
							height: parent.height - startBtn.depth

							color: "white"
							radius: 8

							y: startBtn.pressed ? startBtn.depth : 0

							Behavior on y {
								NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
							}
						}
					}

					contentItem: Item
					{
						Text
						{
							text:
							{
								if (rootTimer.timer.state === PomodoroTimer.Running) return "PAUSE"
								if (rootTimer.timer.state === PomodoroTimer.Paused) return "RESUME"
								return "START"
							}

							font.pixelSize: startBtn.width * 0.18
							font.bold: true
							color: rootTimer.themeColor

							anchors.horizontalCenter: parent.horizontalCenter
							anchors.verticalCenter: parent.verticalCenter

							anchors.verticalCenterOffset: startBtn.pressed ? startBtn.depth : 0

							Behavior on anchors.verticalCenterOffset {
								NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
							}
						}
					}

					onClicked:
						rootTimer.timer.toggle()
				}

				Button
				{
					id: resetBtn

					width: startBtn.height * 0.55
					height: startBtn.height * 0.55

					anchors.verticalCenter: parent.verticalCenter
					anchors.verticalCenterOffset: -startBtn.depth / 2

					// Nothing to go back to while the session is untouched.
					enabled: rootTimer.timer.state !== PomodoroTimer.Idle

					opacity: resetBtn.enabled ? 1.0 : 0.3

					Behavior on opacity
					{
						NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
					}

					background: Rectangle
					{
						radius: width / 2
						color: resetBtn.hovered ? Qt.rgba(1, 1, 1, 0.28) : Qt.rgba(1, 1, 1, 0.12)
						border.color: Qt.rgba(1, 1, 1, 0.3)
						border.width: 1

						Behavior on color
						{
							ColorAnimation { duration: 150 }
						}
					}

					icon.source: "assets/icons/restartTimer.svg"
					icon.color: "white"
					icon.width: resetBtn.width * 0.5
					icon.height: resetBtn.height * 0.5

					onClicked:
						rootTimer.timer.reset()
				}
			}
		}
	}
}
