import QtQuick
import QtQuick.Controls

// The one scroll bar every panel uses: a thin rounded line at the very edge, faint while
// the list is still, clearer while it scrolls or the mouse is on it. The Basic style's
// own bar is wide, grey and sits over the text, which is what made the drawers look
// unfinished.
ScrollBar
{
	id: rootBar

	implicitWidth: 6
	padding: 1

	policy: ScrollBar.AsNeeded
	minimumSize: 0.1

	background: Item {}

	contentItem: Rectangle
	{
		implicitWidth: 4
		radius: 2
		color: "white"

		// Nothing at all when there is nothing to scroll.
		opacity: rootBar.size >= 1.0 ? 0.0
			: rootBar.pressed ? 0.6
			: rootBar.hovered || rootBar.active ? 0.4
			: 0.15

		Behavior on opacity { NumberAnimation { duration: 200 } }
	}
}
