import QtQuick
import QtQuick.Controls

// One labelled row with a slider and a percentage readout. Same contract as the other
// setting rows: it reports what the user asked for and writes nothing itself.
Item
{
	id: rootSlider

	required property string label
	required property real value

	property real from: 0.0
	property real to: 1.0

	signal valueModified(real newValue)
	signal previewRequested()

	implicitHeight: 48
	width: parent ? parent.width : implicitWidth

	Text
	{
		anchors.left: parent.left
		anchors.verticalCenter: parent.verticalCenter

		text: rootSlider.label
		color: "white"
		font.pixelSize: 15
	}

	Text
	{
		id: readout

		width: 42
		anchors.right: parent.right
		anchors.verticalCenter: parent.verticalCenter

		text: Math.round(rootSlider.value * 100) + "%"
		color: "white"
		font.pixelSize: 13
		font.bold: true
		horizontalAlignment: Text.AlignRight
	}

	Slider
	{
		id: slider

		width: 120
		anchors.right: readout.left
		anchors.rightMargin: 10
		anchors.verticalCenter: parent.verticalCenter

		from: rootSlider.from
		to: rootSlider.to

		background: Rectangle
		{
			x: slider.leftPadding
			y: slider.topPadding + slider.availableHeight / 2 - height / 2

			width: slider.availableWidth
			height: 5
			radius: 3

			color: Qt.rgba(1, 1, 1, 0.18)

			Rectangle
			{
				width: slider.visualPosition * parent.width
				height: parent.height
				radius: parent.radius

				color: "white"
				opacity: 0.9
			}
		}

		handle: Rectangle
		{
			x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
			y: slider.topPadding + slider.availableHeight / 2 - height / 2

			width: 18
			height: 18
			radius: width / 2

			color: slider.pressed ? "#eaeaea" : "white"
			border.color: Qt.rgba(1, 1, 1, 0.4)
			border.width: 1
		}

		onMoved:
			rootSlider.valueModified(slider.value)

		// Let go and you hear what you just picked.
		onPressedChanged:
			if (!slider.pressed)
				rootSlider.previewRequested()
	}

	// Dragging writes to the Slider's own value and would destroy a plain binding, so
	// the outside value is pushed back in whenever the handle is not being held.
	Binding
	{
		target: slider
		property: "value"
		value: rootSlider.value
		when: !slider.pressed
	}
}
