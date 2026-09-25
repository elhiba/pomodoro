pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

// The three music sources as icons -- the station list, YouTube and Spotify -- with the
// current one ringed. A link of one's own is added to the station list with its "+"
// tile rather than being a source of its own. Only reports the pick; the owner writes it.
Row
{
	id: rootTabs

	required property string current

	// Each tab's size. The settings drawer stretches them to its width instead.
	property real tabWidth: 44
	property real tabHeight: 44

	signal picked(string key)

	spacing: 8

	Repeater
	{
		model: [
			{ key: "radio", icon: "assets/icons/radio.svg", label: "Lo-fi radio" },
			{ key: "youtube", icon: "assets/icons/youtube.svg", label: "YouTube" },
			{ key: "spotify", icon: "assets/icons/spotify.svg", label: "Spotify" }
		]

		delegate: Rectangle
		{
			id: tab

			required property var modelData

			readonly property bool selected: rootTabs.current === tab.modelData.key

			width: rootTabs.tabWidth
			height: rootTabs.tabHeight
			radius: 12

			color: tab.selected
				? Qt.rgba(1, 1, 1, 0.26)
				: tabHover.hovered ? Qt.rgba(1, 1, 1, 0.14) : Qt.rgba(1, 1, 1, 0.06)

			border.color: tab.selected ? Qt.rgba(1, 1, 1, 0.7) : "transparent"
			border.width: 1.5

			Behavior on color
			{
				ColorAnimation { duration: 150 }
			}

			// Drawn as an Image rather than a button icon: a button would tint the brand
			// colours away.
			Image
			{
				anchors.centerIn: parent

				width: Math.min(tab.width, tab.height) * 0.52
				height: width

				source: tab.modelData.icon
				sourceSize.width: 64
				sourceSize.height: 64
				fillMode: Image.PreserveAspectFit
				smooth: true
			}

			HoverHandler
			{
				id: tabHover
				cursorShape: Qt.PointingHandCursor
			}

			TapHandler
			{
				onTapped:
					rootTabs.picked(tab.modelData.key)
			}

			ToolTip.visible: tabHover.hovered
			ToolTip.delay: 500
			ToolTip.text: tab.modelData.label
		}
	}
}
