import QtQuick

// A seek bar in the style of Samsung's One UI media player (the live notification): a
// thick rounded track, filled solid up to a round thumb, with a soft, translucent wave
// rising out of the filled part -- low hills that drift slowly while the music plays and
// sink back into the track when it pauses. Dragging it moves the thumb; letting go asks
// for the jump.
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

	// Where the thumb is shown: under the finger while dragging, else at value.
	readonly property real shownValue: rootSlider.dragging ? dragArea.dragValue : rootSlider.value

	// Reports the chosen point, 0 to 1, once the drag or tap is over.
	signal seekRequested(real fraction)

	implicitHeight: 30

	// The wave's height and drift. The height eases in and out rather than snapping, so
	// pausing looks like the music settling.
	property real amplitude: rootSlider.playing && !rootSlider.dragging ? 1 : 0
	property real phase: 0

	Behavior on amplitude
	{
		NumberAnimation { duration: 600; easing.type: Easing.InOutQuad }
	}

	onAmplitudeChanged: canvas.requestPaint()
	onPhaseChanged: canvas.requestPaint()
	onShownValueChanged: canvas.requestPaint()
	onWidthChanged: canvas.requestPaint()

	Timer
	{
		interval: 40
		repeat: true
		running: rootSlider.visible && rootSlider.amplitude > 0

		// Slow: the hills take several seconds to drift across.
		onTriggered: rootSlider.phase = (rootSlider.phase + 0.05) % (Math.PI * 200)
	}

	Canvas
	{
		id: canvas

		anchors.fill: parent

		onPaint:
		{
			let context = getContext("2d")
			let track = 8
			let thumb = 16
			let inset = thumb / 2
			let middle = height - thumb / 2 - 2
			let top = middle - track / 2
			let length = width - inset * 2
			let marker = inset + length * Math.max(0, Math.min(1, rootSlider.shownValue))

			context.reset()

			// The track, and the part already played over it.
			context.fillStyle = Qt.rgba(1, 1, 1, 0.2)
			context.beginPath()
			context.roundedRect(inset - track / 2, top, length + track, track, track / 2, track / 2)
			context.fill()

			if (marker > inset)
			{
				context.fillStyle = Qt.rgba(1, 1, 1, 0.85)
				context.beginPath()
				context.roundedRect(inset - track / 2, top, marker - inset + track, track, track / 2, track / 2)
				context.fill()
			}

			// The wave: two see-through waves over the played part, each drawn as its own
			// layer, as in One UI. They have different lengths and drift at different
			// speeds, so they slide over and through each other, and where they cross the
			// overlap shows brighter. Both are pinned to the track at the ends of the
			// played part.
			let peak = (top - 1) * rootSlider.amplitude
			let played = marker - inset

			if (peak > 0.5 && played > 12)
			{
				let phase = rootSlider.phase
				let layers = [
					{ length: 1.3, speed: 1.0, height: 1.0, offset: 0 },
					{ length: 0.9, speed: -0.7, height: 0.75, offset: 2.1 }
				]

				context.fillStyle = Qt.rgba(1, 1, 1, 0.22)

				for (let layer of layers)
				{
					context.beginPath()
					context.moveTo(inset, top + 1)

					for (let x = inset; x <= marker; x += 3)
					{
						let along = (x - inset) / played

						// About one to two gentle crests over the played part, whatever
						// its length, rising from the track and settling back into it.
						let crest = 0.5 + 0.5 * Math.sin(along * Math.PI * 2 * layer.length
							- phase * layer.speed + layer.offset)

						context.lineTo(x, top + 1 - peak * layer.height * crest * Math.sin(Math.PI * along))
					}

					context.lineTo(marker, top + 1)
					context.closePath()
					context.fill()
				}
			}

			// The thumb: a round knob, a little larger while held.
			let knob = rootSlider.dragging ? thumb + 4 : thumb

			context.fillStyle = "white"
			context.beginPath()
			context.ellipse(marker - knob / 2, middle - knob / 2, knob, knob)
			context.fill()

			context.strokeStyle = Qt.rgba(0, 0, 0, 0.12)
			context.lineWidth = 1
			context.stroke()
		}
	}

	MouseArea
	{
		id: dragArea

		property real dragValue: 0

		anchors.fill: parent
		anchors.topMargin: -4
		anchors.bottomMargin: -6

		cursorShape: Qt.PointingHandCursor
		preventStealing: true

		function fractionAt(x)
		{
			return Math.max(0, Math.min(1, (x - 8) / Math.max(1, width - 16)))
		}

		onPressed: (mouse) =>
		{
			dragArea.dragValue = dragArea.fractionAt(mouse.x)
			canvas.requestPaint()
		}

		onPositionChanged: (mouse) =>
		{
			if (pressed)
				dragArea.dragValue = dragArea.fractionAt(mouse.x)
		}

		onReleased: rootSlider.seekRequested(dragArea.dragValue)
	}
}
