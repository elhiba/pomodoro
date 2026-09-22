// The delegate below is a nested component, so it needs bound component behaviour
// to reach rootTabs. Without this the id resolves through the context by luck.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import Pomodoro

// Manual switching between the three session kinds. Picking one restarts that
// session from the top, which is what PomodoroTimer.setMode already does.
Row
{
	id: rootTabs

	required property PomodoroTimer timer

	spacing: 8

	Repeater
	{
		model: [
			{ label: "Focus", mode: PomodoroTimer.Focus },
			{ label: "Short Break", mode: PomodoroTimer.ShortBreak },
			{ label: "Long Break", mode: PomodoroTimer.LongBreak }
		]

		delegate: Button
		{
			id: tabBtn

			required property var modelData

			readonly property bool current: rootTabs.timer.mode === tabBtn.modelData.mode

			height: 34
			leftPadding: 15
			rightPadding: 15

			background: Rectangle
			{
				radius: 8

				color: tabBtn.current
					? Qt.rgba(1, 1, 1, 0.28)
					: tabBtn.hovered ? Qt.rgba(1, 1, 1, 0.12) : "transparent"

				Behavior on color
				{
					ColorAnimation { duration: 150 }
				}
			}

			contentItem: Text
			{
				text: tabBtn.modelData.label
				color: "white"
				font.pixelSize: 14
				font.bold: tabBtn.current
				opacity: tabBtn.current ? 1.0 : 0.75

				horizontalAlignment: Text.AlignHCenter
				verticalAlignment: Text.AlignVCenter
			}

			onClicked:
			{
				if (tabBtn.current) return
				SoundPlayer.playClick()
				rootTabs.timer.setMode(tabBtn.modelData.mode)
			}
		}
	}
}
