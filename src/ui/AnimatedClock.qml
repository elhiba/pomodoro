pragma ComponentBehavior: Bound

import QtQuick

// The timer's digits, each in a slot of its own that animates only when its own digit
// changes, so a tick from 24:59 to 24:58 moves one digit, not the whole clock.
//
//   ""     still: the digits simply change, as they always have
//   "roll" the old digit rolls up and away as the new one rolls in from below
//   "flip" the old digit folds shut and the new one opens, like a flip clock
//   "soft" the old digit fades and grows away as the new one settles into place
//
// Every slot is as wide as the font's widest digit, so a proportional font does not make
// the clock shuffle sideways as the numbers change. The animations run once per changed
// digit and finish within a third of a second: between ticks nothing moves.
Row
{
	id: rootClock

	property string text: ""
	property font font
	property color color: "white"
	property string style: ""

	// Follows its font binding; AppFont leaves it alone.
	property bool ownFont: true

	FontMetrics
	{
		id: metrics
		font: rootClock.font
	}

	// TextMetrics rather than FontMetrics.advanceWidth(): its width is a property, so it is
	// measured again when the font changes. Measured once through the function, before
	// the big font arrived, every slot came out a few pixels wide and the digits piled up
	// on top of each other. Digits are nearly always the same width in a font; the wider
	// of 0 and 8 covers the rest.
	TextMetrics
	{
		id: zero
		font: rootClock.font
		text: "0"
	}

	TextMetrics
	{
		id: eight
		font: rootClock.font
		text: "8"
	}

	readonly property real digitWidth: Math.ceil(Math.max(zero.advanceWidth, eight.advanceWidth))

	Repeater
	{
		model: rootClock.text.length

		delegate: Item
		{
			id: slot

			required property int index

			readonly property string character: rootClock.text.charAt(slot.index)
			readonly property bool digit: slot.character >= "0" && slot.character <= "9"

			property string current: slot.character
			property string previous: ""

			width: slot.digit ? rootClock.digitWidth : Math.ceil(own.advanceWidth)
			height: Math.ceil(metrics.height)

			TextMetrics
			{
				id: own
				font: rootClock.font
				text: slot.character
			}

			// Rolling digits pass out of their slot; the others stay inside it.
			clip: rootClock.style === "roll"

			onCharacterChanged:
			{
				if (slot.character === slot.current)
					return

				slot.previous = slot.current
				slot.current = slot.character

				roll.stop()
				flip.stop()
				soft.stop()
				slot.settle()

				if (rootClock.style === "roll")
					roll.start()
				else if (rootClock.style === "flip")
					flip.start()
				else if (rootClock.style === "soft")
					soft.start()
			}

			// Everything back where a still clock has it.
			function settle()
			{
				incoming.y = 0
				incoming.opacity = 1
				incoming.scale = 1
				incomingScale.yScale = 1
				outgoing.y = 0
				outgoing.opacity = 0
				outgoing.scale = 1
				outgoingScale.yScale = 1
			}

			Text
			{
				id: incoming

				x: (slot.width - implicitWidth) / 2
				text: slot.current
				color: rootClock.color
				font: rootClock.font

				transform: Scale
				{
					id: incomingScale
					origin.y: slot.height / 2
				}
			}

			Text
			{
				id: outgoing

				x: (slot.width - implicitWidth) / 2
				text: slot.previous
				color: rootClock.color
				font: rootClock.font
				opacity: 0

				transform: Scale
				{
					id: outgoingScale
					origin.y: slot.height / 2
				}
			}

			ParallelAnimation
			{
				id: roll

				NumberAnimation { target: incoming; property: "y"; from: slot.height * 0.9; to: 0; duration: 380; easing.type: Easing.OutCubic }
				NumberAnimation { target: incoming; property: "opacity"; from: 0; to: 1; duration: 260 }
				NumberAnimation { target: outgoing; property: "y"; from: 0; to: -slot.height * 0.9; duration: 380; easing.type: Easing.OutCubic }
				NumberAnimation { target: outgoing; property: "opacity"; from: 1; to: 0; duration: 300 }
			}

			SequentialAnimation
			{
				id: flip

				ScriptAction { script: { outgoing.opacity = 1; incomingScale.yScale = 0 } }
				NumberAnimation { target: outgoingScale; property: "yScale"; from: 1; to: 0; duration: 140; easing.type: Easing.InQuad }
				ScriptAction { script: outgoing.opacity = 0 }
				NumberAnimation { target: incomingScale; property: "yScale"; from: 0; to: 1; duration: 170; easing.type: Easing.OutBack }
			}

			ParallelAnimation
			{
				id: soft

				NumberAnimation { target: incoming; property: "opacity"; from: 0; to: 1; duration: 320; easing.type: Easing.OutQuad }
				NumberAnimation { target: incoming; property: "scale"; from: 0.82; to: 1; duration: 320; easing.type: Easing.OutBack }
				NumberAnimation { target: outgoing; property: "opacity"; from: 1; to: 0; duration: 260; easing.type: Easing.OutQuad }
				NumberAnimation { target: outgoing; property: "scale"; from: 1; to: 1.25; duration: 260; easing.type: Easing.OutQuad }
			}
		}
	}
}
