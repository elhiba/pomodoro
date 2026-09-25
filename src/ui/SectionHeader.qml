import QtQuick

// A heading that folds what is under it away: the title, an optional one-line summary of
// the current choice while folded, and a chevron that points right when closed and down
// when open. Tapping anywhere on it toggles. The owner shows or hides the content from
// `expanded`, so a long settings page stays a short list until something is opened.
Item
{
	id: rootHeader

	required property string title

	// Shown beside the title while folded, e.g. the chosen font's name.
	property string summary: ""

	// Section titles are small capitals; groups inside a section are ordinary rows.
	property bool small: true

	property bool expanded: false

	width: parent ? parent.width : implicitWidth
	implicitHeight: rootHeader.small ? 40 : 46

	Rectangle
	{
		anchors.fill: parent
		anchors.leftMargin: -8
		anchors.rightMargin: -8
		radius: 8
		color: headerHover.hovered ? Qt.rgba(1, 1, 1, 0.07) : "transparent"
	}

	Text
	{
		id: titleText

		anchors.left: parent.left
		anchors.verticalCenter: parent.verticalCenter

		text: rootHeader.title
		color: rootHeader.small ? Qt.rgba(1, 1, 1, 0.6) : "white"
		font.pixelSize: rootHeader.small ? 11 : 15
		font.bold: rootHeader.small
		font.letterSpacing: rootHeader.small ? 1.2 : 0
	}

	Text
	{
		anchors.left: titleText.right
		anchors.leftMargin: 10
		anchors.right: chevron.left
		anchors.rightMargin: 10
		anchors.verticalCenter: parent.verticalCenter

		visible: !rootHeader.expanded && rootHeader.summary.length > 0
		text: rootHeader.summary
		textFormat: Text.PlainText
		horizontalAlignment: Text.AlignRight
		color: Qt.rgba(1, 1, 1, 0.5)
		font.pixelSize: 13
		elide: Text.ElideRight
	}

	Text
	{
		id: chevron

		anchors.right: parent.right
		anchors.verticalCenter: parent.verticalCenter

		text: "›"
		color: Qt.rgba(1, 1, 1, 0.7)
		font.pixelSize: 22
		rotation: rootHeader.expanded ? 90 : 0

		Behavior on rotation
		{
			NumberAnimation { duration: 180; easing.type: Easing.OutQuad }
		}
	}

	HoverHandler
	{
		id: headerHover
		cursorShape: Qt.PointingHandCursor
	}

	TapHandler
	{
		onTapped: rootHeader.expanded = !rootHeader.expanded
	}
}
