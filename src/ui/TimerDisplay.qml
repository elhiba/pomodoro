import QtQuick
import QtQuick.Controls

import Pomodoro

Item
{
    id: rootTimer

	// Sized and fed from Main.qml, so this component never reaches out to the window.
	required property PomodoroTimer timer
	property color themeColor: "#12130F"

	// Asks for the shown session to be this many minutes long. Main.qml writes it to
	// AppSettings, which feeds the timer as it always has, so this panel never changes
	// a preference by itself.
	signal minutesRequested(int minutes)

	// Only a session that has not started may be re-lengthened, the same rule the timer
	// itself applies.
	readonly property bool editable: rootTimer.timer.state === PomodoroTimer.Idle
	readonly property int minutes: Math.round(rootTimer.timer.totalSeconds / 60)

	// Single letter shortcuts have to stand down while the minutes are being typed.
	readonly property bool typing: minutesInput.activeFocus

	// A round - or + drawn from two bars, so it needs no icon file and stays crisp at any
	// size. Faded out rather than hidden when disabled, to keep the digits where they are.
	component StepButton: Button
	{
		id: stepBtn

		// Handed in rather than read from the outer ids, which an inline component
		// cannot see.
		property bool plus: true
		property bool shown: true
		property real size: 32

		width: stepBtn.size
		height: stepBtn.size

		autoRepeat: true
		autoRepeatDelay: 400
		autoRepeatInterval: 90

		opacity: stepBtn.enabled ? 1.0 : (stepBtn.shown ? 0.3 : 0.0)

		Behavior on opacity
		{
			NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
		}

		background: Rectangle
		{
			radius: width / 2
			color: stepBtn.pressed
				? Qt.rgba(1, 1, 1, 0.34)
				: stepBtn.hovered ? Qt.rgba(1, 1, 1, 0.24) : Qt.rgba(1, 1, 1, 0.12)
			border.color: Qt.rgba(1, 1, 1, 0.3)
			border.width: 1
		}

		contentItem: Item
		{
			Rectangle
			{
				anchors.centerIn: parent
				width: stepBtn.width * 0.42
				height: Math.max(2, stepBtn.width * 0.08)
				radius: height / 2
				color: "white"
			}

			Rectangle
			{
				anchors.centerIn: parent
				visible: stepBtn.plus
				width: Math.max(2, stepBtn.width * 0.08)
				height: stepBtn.width * 0.42
				radius: width / 2
				color: "white"
			}
		}

		onPressed:
			SoundPlayer.playClick()
	}

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

			// Was a flat 30. Scaled with the panel so a small window keeps its
			// breathing room around the button instead of spending it all on the gap.
			// At the default panel height this still works out at 30.
			spacing: Math.max(16, glassPanel.height * 0.111)

			// The digits, with the session's length editable in place while it has not
			// started: - and + beside them (hold to repeat), the mouse wheel over them, or
			// a click to type the minutes. Hidden rather than removed while a session runs,
			// so the digits never shift sideways when START is pressed.
			Row
			{
				id: timeRow

				anchors.horizontalCenter: parent.horizontalCenter
				spacing: timeText.font.pixelSize * 0.18

				StepButton
				{
					id: lessBtn

					anchors.verticalCenter: parent.verticalCenter
					size: timeText.font.pixelSize * 0.42
					shown: rootTimer.editable
					plus: false

					enabled: rootTimer.editable && rootTimer.minutes > AppSettings.minimumMinutes

					onClicked:
						rootTimer.minutesRequested(rootTimer.minutes - 1)
				}

				Item
				{
					width: Math.max(timeText.implicitWidth, clock.width)
					height: timeText.implicitHeight

					// The digits and the minutes field bind their font to the setting.
					property bool ownFont: true

					// Measures and sizes the clock, and is what the typed-minutes field
					// copies its font from; the digits on screen are the AnimatedClock.
					Text
					{
						id: timeText

						anchors.centerIn: parent

						text: rootTimer.timer.displayTime
						color: "white"
						opacity: 0
						font.family: AppSettings.appFont.length > 0 ? AppSettings.appFont : timerFont.name
						// Sized from whichever is the real constraint: the panel's width, or
						// the width its height would allow at 16:9. Using the width alone
						// made the digits too tall for a short, wide panel, and the column
						// below them pushed the start button out through the bottom edge.
						font.pixelSize: Math.min(glassPanel.width, glassPanel.height / 0.5625) * 0.2

					}

					AnimatedClock
					{
						id: clock

						anchors.centerIn: parent

						text: rootTimer.timer.displayTime
						font: timeText.font
						style: AppSettings.timerStyle
						visible: !minutesInput.visible
					}

					// Only the minutes are typed; seconds are always :00 on a fresh session.
					TextInput
					{
						id: minutesInput

						anchors.centerIn: parent

						visible: false

						color: "white"
						selectionColor: Qt.rgba(1, 1, 1, 0.35)
						selectedTextColor: "white"
						font: timeText.font
						horizontalAlignment: TextInput.AlignHCenter
						maximumLength: 3
						inputMethodHints: Qt.ImhDigitsOnly

						validator: IntValidator
						{
							bottom: AppSettings.minimumMinutes
							top: AppSettings.maximumMinutes
						}

						function begin()
						{
							minutesInput.text = rootTimer.minutes
							minutesInput.visible = true
							minutesInput.forceActiveFocus()
							minutesInput.selectAll()
						}

						function finish(keep)
						{
							if (!minutesInput.visible)
								return

							minutesInput.visible = false

							let typed = parseInt(minutesInput.text)

							if (keep && !isNaN(typed))
								rootTimer.minutesRequested(typed)
						}

						onAccepted:
							minutesInput.finish(true)

						onActiveFocusChanged:
							if (!minutesInput.activeFocus)
								minutesInput.finish(true)

						Keys.onEscapePressed:
							minutesInput.finish(false)
					}

					MouseArea
					{
						anchors.fill: parent

						enabled: rootTimer.editable && !minutesInput.visible
						cursorShape: enabled ? Qt.IBeamCursor : Qt.ArrowCursor

						onClicked:
							minutesInput.begin()

						// One minute per notch, whichever way the wheel reports it.
						onWheel: (wheel) =>
						{
							let step = wheel.angleDelta.y > 0 ? 1 : wheel.angleDelta.y < 0 ? -1 : 0

							if (step !== 0)
								rootTimer.minutesRequested(rootTimer.minutes + step)
						}
					}

					ToolTip.visible: timeHover.hovered && rootTimer.editable && !minutesInput.visible
					ToolTip.delay: 700
					ToolTip.text: "Click to type the minutes, or scroll to adjust"

					HoverHandler
					{
						id: timeHover
					}
				}

				StepButton
				{
					id: moreBtn

					anchors.verticalCenter: parent.verticalCenter
					size: timeText.font.pixelSize * 0.42
					shown: rootTimer.editable
					plus: true

					enabled: rootTimer.editable && rootTimer.minutes < AppSettings.maximumMinutes

					onClicked:
						rootTimer.minutesRequested(rootTimer.minutes + 1)
				}
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
					{
						SoundPlayer.playClick()
						rootTimer.timer.toggle()
					}
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
					{
						SoundPlayer.playClick()
						rootTimer.timer.reset()
					}
				}
			}
		}
	}
}
