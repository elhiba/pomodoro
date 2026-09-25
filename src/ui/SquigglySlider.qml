import QtQuick

// A seek bar in the style of Android's media player: the part already played is a wave
// that rolls gently while the music plays and flattens out when it pauses, the rest is a
// thin straight line, and a short upright bar marks where playback is. Dragging it moves
// the marker; letting go asks for the jump.
//
// Drawn on a Canvas and only animated while it is on screen and playing, a frame every
// 40 ms -- the rest of the time it is a still image, so an open panel with paused music
// costs nothing.
Item
{
	id: rootSlider

	// 0 to 1: how far through the song playback is.
	property real value: 0
	property bool playing: false

	readonly property bool dragging: dragArea.pressed

	// Where the marker is shown: under the finger while dragging, else at value.
	readonly property real shownValue: rootSlider.dragging ? dragArea.dragValue : rootSlider.value

	// Reports the chosen point, 0 to 1, once the drag or tap is over.
	signal seekRequested(real fraction)

	implicitHeight: 24

	// The wave's height and roll. The height eases in and out rather than snapping, which
	// is what makes pausing feel like the music settling.
	property real amplitude: rootSlider.playing && !rootSlider.dragging ? 3 : 0
	property real phase: 0

	Behavior on amplitude
	{
		NumberAnimation { duration: 400; easing.type: Easing.InOutQuad }
	}

	onAmplitudeChanged: wave.requestPaint()
	onPhaseChanged: wave.requestPaint()
	onShownValueChanged: wave.requestPaint()
	onWidthChanged: wave.requestPaint()

	Timer
	{
		interval: 40
		repeat: true
		running: rootSlider.visible && rootSlider.amplitude > 0

		// One wavelength every ~1.5 s: a slow roll, not a wiggle.
		onTriggered: rootSlider.phase = (rootSlider.phase + 0.17) % (Math.PI * 2)
	}

	Canvas
	{
		id: wave

		anchors.fill: parent

		onPaint:
		{
			let context = getContext("2d")
			let middle = height / 2
			let inset = 3
			let track = width - inset * 2
			let marker = inset + track * Math.max(0, Math.min(1, rootSlider.shownValue))
			let wavelength = 22

			context.reset()
			context.lineCap = "round"
			context.lineWidth = 3

			// What is left: a thin straight line, starting a little after the marker.
			if (marker + 6 < width - inset)
			{
				context.strokeStyle = Qt.rgba(1, 1, 1, 0.3)
				context.beginPath()
				context.moveTo(marker + 6, middle)
				context.lineTo(width - inset, middle)
				context.stroke()
			}

			// What has played: the wave, up to just before the marker.
			if (marker - 4 > inset)
			{
				context.strokeStyle = "white"
				context.beginPath()

				for (let x = inset; x <= marker - 4; x += 1)
				{
					let y = middle + rootSlider.amplitude
						* Math.sin((x / wavelength) * Math.PI * 2 - rootSlider.phase)

					if (x === inset)
						context.moveTo(x, y)
					else
						context.lineTo(x, y)
				}

				context.stroke()
			}

			// The marker: an upright rounded bar, taller while held.
			let barHeight = rootSlider.dragging ? 18 : 14

			context.fillStyle = "white"
			context.beginPath()
			context.roundedRect(marker - 2, middle - barHeight / 2, 4, barHeight, 2, 2)
			context.fill()
		}
	}

	MouseArea
	{
		id: dragArea

		property real dragValue: 0

		anchors.fill: parent
		anchors.topMargin: -6
		anchors.bottomMargin: -6

		cursorShape: Qt.PointingHandCursor
		preventStealing: true

		function fractionAt(x)
		{
			return Math.max(0, Math.min(1, (x - 3) / Math.max(1, width - 6)))
		}

		onPressed: (mouse) =>
		{
			dragArea.dragValue = dragArea.fractionAt(mouse.x)
			wave.requestPaint()
		}

		onPositionChanged: (mouse) =>
		{
			if (pressed)
				dragArea.dragValue = dragArea.fractionAt(mouse.x)
		}

		onReleased: rootSlider.seekRequested(dragArea.dragValue)
	}
}
