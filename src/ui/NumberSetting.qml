import QtQuick
import QtQuick.Controls

// One labelled row with a minus / value / plus stepper on the right.
// It never writes anywhere itself, it just reports what the user asked for.
Item
{
	id: rootSetting

	required property string label
	required property int value

	property int minimum: 1
	property int maximum: 120
	property int step: 1
	property string suffix: "min"

	signal valueModified(int newValue)

	implicitHeight: 48
	width: parent ? parent.width : implicitWidth

	Text
	{
		anchors.left: parent.left
		anchors.verticalCenter: parent.verticalCenter

		text: rootSetting.label
		color: "white"
		font.pixelSize: 15
	}

	Row
	{
		anchors.right: parent.right
		anchors.verticalCenter: parent.verticalCenter

		spacing: 6

		Button
		{
			id: minusBtn

			width: 30
			height: 30
			anchors.verticalCenter: parent.verticalCenter

			enabled: rootSetting.value > rootSetting.minimum
			opacity: minusBtn.enabled ? 1.0 : 0.3

			background: Rectangle
			{
				radius: width / 2
				color: minusBtn.hovered ? Qt.rgba(1, 1, 1, 0.28) : Qt.rgba(1, 1, 1, 0.12)
				border.color: Qt.rgba(1, 1, 1, 0.3)
				border.width: 1

				Behavior on color
				{
					ColorAnimation { duration: 150 }
				}
			}

			contentItem: Text
			{
				text: "−"
				color: "white"
				font.pixelSize: 17
				horizontalAlignment: Text.AlignHCenter
				verticalAlignment: Text.AlignVCenter
			}

			onClicked:
				rootSetting.valueModified(rootSetting.value - rootSetting.step)
		}

		Text
		{
			width: 62
			anchors.verticalCenter: parent.verticalCenter

			text: rootSetting.suffix.length > 0
				? rootSetting.value + " " + rootSetting.suffix
				: String(rootSetting.value)

			color: "white"
			font.pixelSize: 15
			font.bold: true
			horizontalAlignment: Text.AlignHCenter
		}

		Button
		{
			id: plusBtn

			width: 30
			height: 30
			anchors.verticalCenter: parent.verticalCenter

			enabled: rootSetting.value < rootSetting.maximum
			opacity: plusBtn.enabled ? 1.0 : 0.3

			background: Rectangle
			{
				radius: width / 2
				color: plusBtn.hovered ? Qt.rgba(1, 1, 1, 0.28) : Qt.rgba(1, 1, 1, 0.12)
				border.color: Qt.rgba(1, 1, 1, 0.3)
				border.width: 1

				Behavior on color
				{
					ColorAnimation { duration: 150 }
				}
			}

			contentItem: Text
			{
				text: "+"
				color: "white"
				font.pixelSize: 17
				horizontalAlignment: Text.AlignHCenter
				verticalAlignment: Text.AlignVCenter
			}

			onClicked:
				rootSetting.valueModified(rootSetting.value + rootSetting.step)
		}
	}
}
