import QtQuick

import Pomodoro

// The small floating timer that stands in for the main window while it is minimised
// (issue #1): the time left and nothing else, always on top, draggable anywhere, and a
// click brings the full window back.
//
// A separate top-level window rather than a shrunken main one, so minimising keeps its
// usual meaning to the desktop -- the app stays in the taskbar and Alt+Tab -- and this
// one stays out of both (Qt.Tool).
Window
{
	id: rootMini

	required property PomodoroTimer timer
	property color themeColor: "#12130F"

	// Asked to bring the main window back.
	signal restoreRequested()

	// Sized from the digits, so a longer session ("100:00") or a wider font still fits.
	readonly property int wantedWidth: Math.max(168, Math.ceil(timeLabel.implicitWidth) + 64)
	readonly property int wantedHeight: 60

	width: rootMini.wantedWidth
	height: rootMini.wantedHeight

	// Moved to a monitor with a different scale, Windows can keep the window's old size
	// in pixels while the contents are drawn at the new scale, and the digits spill out
	// of a card that is suddenly too small. The size is put back once the move settles:
	// nudged by a pixel first, because asking for the size it already has does nothing.
	onScreenChanged: Qt.callLater(rootMini.reassertSize)

	function reassertSize()
	{
		rootMini.width = rootMini.wantedWidth + 1
		rootMini.height = rootMini.wantedHeight + 1
		rootMini.width = Qt.binding(() => rootMini.wantedWidth)
		rootMini.height = Qt.binding(() => rootMini.wantedHeight)
	}

	flags: Qt.Tool | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint | Qt.WindowDoesNotAcceptFocus

	// Transparent so the rounded card below is the window's real shape.
	color: "transparent"

	// A stored position is only trusted if it still lands on a screen; a monitor that has
	// since been unplugged would otherwise strand the timer somewhere nobody can see.
	// Never placed, or placed off screen: bottom right, just above the taskbar. Not the
	// top right, where it would sit on every maximised window's close button.
	function place()
	{
		let left = Screen.virtualX
		let top = Screen.virtualY
		let right = left + Screen.desktopAvailableWidth
		let bottom = top + Screen.desktopAvailableHeight

		let x = AppSettings.miniTimerX
		let y = AppSettings.miniTimerY

		if (x < left || y < top || x + rootMini.width > right || y + rootMini.height > bottom)
		{
			x = right - rootMini.width - 24
			y = bottom - rootMini.height - 24
		}

		rootMini.x = x
		rootMini.y = y
	}

	onVisibleChanged:
		if (rootMini.visible)
			rootMini.place()

	Rectangle
	{
		id: card

		anchors.fill: parent

		radius: 14
		color: rootMini.themeColor
		border.color: Qt.rgba(1, 1, 1, 0.3)
		border.width: 1

		Behavior on color
		{
			ColorAnimation { duration: 500; easing.type: Easing.InOutQuad }
		}

		FontLoader
		{
			id: timerFont
			source: "assets/fonts/JetBrainsMono.ttf"
		}

		// A dot for the kind of session, dimmed while it is not counting down.
		Rectangle
		{
			id: stateDot

			anchors.left: parent.left
			anchors.leftMargin: 16
			anchors.verticalCenter: parent.verticalCenter

			width: 8
			height: 8
			radius: 4

			color: "white"
			opacity: rootMini.timer.state === PomodoroTimer.Running ? 0.95 : 0.35
		}

		Text
		{
			id: timeLabel

			anchors.centerIn: parent
			anchors.horizontalCenterOffset: 8

			text: rootMini.timer.displayTime
			color: "white"
			font.family: AppSettings.timerFont.length > 0 ? AppSettings.timerFont : timerFont.name
			font.pixelSize: 30
			font.bold: true
		}

		// The same progress as the main window's bar, along the bottom edge.
		Rectangle
		{
			anchors.left: parent.left
			anchors.bottom: parent.bottom
			anchors.leftMargin: card.radius
			anchors.bottomMargin: 6

			width: (parent.width - 2 * card.radius) * rootMini.timer.progress
			height: 3
			radius: 1.5

			color: "white"
			opacity: 0.7
		}
	}

	// Press and move to drag, press and release in place to restore. The drag is handed
	// to the window manager as soon as the pointer moves, which is what lets it snap and
	// cross monitors like any other window.
	MouseArea
	{
		id: dragArea

		anchors.fill: parent

		property point pressedAt
		property bool dragging: false

		cursorShape: Qt.PointingHandCursor

		onPressed: (mouse) =>
		{
			dragArea.pressedAt = Qt.point(mouse.x, mouse.y)
			dragArea.dragging = false
		}

		onPositionChanged: (mouse) =>
		{
			if (!dragArea.pressed || dragArea.dragging)
				return

			if (Math.abs(mouse.x - dragArea.pressedAt.x) + Math.abs(mouse.y - dragArea.pressedAt.y) > 4)
			{
				dragArea.dragging = true
				rootMini.startSystemMove()
			}
		}

		onReleased:
		{
			if (!dragArea.dragging)
				rootMini.restoreRequested()
		}
	}

	// Remembered once the window settles rather than on every pixel of a drag, which
	// would write the settings file dozens of times a second.
	Timer
	{
		id: savePosition

		interval: 500

		onTriggered:
		{
			AppSettings.miniTimerX = rootMini.x
			AppSettings.miniTimerY = rootMini.y
		}
	}

	onXChanged:
		if (rootMini.visible)
			savePosition.restart()

	onYChanged:
		if (rootMini.visible)
			savePosition.restart()
}
