import QtQuick

// One labelled row with a pill switch on the right. Like NumberSetting it only
// reports the request, the panel decides what to do with it.
Item
{
	id: rootToggle

	required property string label
	required property bool checked

	property color accentColor: "#12130F"

	signal toggleRequested(bool wanted)

	implicitHeight: 48
	width: parent ? parent.width : implicitWidth

	Text
	{
		anchors.left: parent.left
		anchors.right: track.left
		anchors.rightMargin: 12
		anchors.verticalCenter: parent.verticalCenter

		text: rootToggle.label
		color: "white"
		font.pixelSize: 15
		elide: Text.ElideRight
	}

	Rectangle
	{
		id: track

		width: 48
		height: 27
		radius: height / 2

		anchors.right: parent.right
		anchors.verticalCenter: parent.verticalCenter

		color: rootToggle.checked ? Qt.rgba(1, 1, 1, 0.85) : Qt.rgba(1, 1, 1, 0.15)
		border.color: Qt.rgba(1, 1, 1, 0.35)
		border.width: 1

		Behavior on color
		{
			ColorAnimation { duration: 150 }
		}

		Rectangle
		{
			id: knob

			width: 21
			height: 21
			radius: height / 2

			y: 3
			x: rootToggle.checked ? track.width - width - 3 : 3

			color: rootToggle.checked ? rootToggle.accentColor : "white"

			Behavior on x
			{
				NumberAnimation { duration: 150; easing.type: Easing.OutQuad }
			}

			Behavior on color
			{
				ColorAnimation { duration: 150 }
			}
		}

		MouseArea
		{
			anchors.fill: parent
			cursorShape: Qt.PointingHandCursor

			onClicked:
				rootToggle.toggleRequested(!rootToggle.checked)
		}
	}
}
