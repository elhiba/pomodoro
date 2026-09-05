// The delegate is a nested component and reaches rootHandles.
pragma ComponentBehavior: Bound

import QtQuick

// Frameless windows lose the decorations the window manager would normally give them,
// including the invisible border you grab to resize. These eight strips put that border
// back without putting the title bar back: each one hands the gesture straight to the
// compositor with startSystemResize, exactly as TopMenu does with startSystemMove.
//
// They stay invisible on purpose. The only thing that gives them away is the cursor.
Item
{
	id: rootHandles

	// How far in from the edge a press still counts as a resize.
	property int margin: 6

	// The corners need a bigger target than the edges, or they are almost unhittable.
	property int cornerSize: 14

	anchors.fill: parent

	Repeater
	{
		model: [
			// edges
			{ edge: Qt.TopEdge,                     cursor: Qt.SizeVerCursor },
			{ edge: Qt.BottomEdge,                  cursor: Qt.SizeVerCursor },
			{ edge: Qt.LeftEdge,                    cursor: Qt.SizeHorCursor },
			{ edge: Qt.RightEdge,                   cursor: Qt.SizeHorCursor },
			// corners
			{ edge: Qt.TopEdge | Qt.LeftEdge,       cursor: Qt.SizeFDiagCursor },
			{ edge: Qt.TopEdge | Qt.RightEdge,      cursor: Qt.SizeBDiagCursor },
			{ edge: Qt.BottomEdge | Qt.LeftEdge,    cursor: Qt.SizeBDiagCursor },
			{ edge: Qt.BottomEdge | Qt.RightEdge,   cursor: Qt.SizeFDiagCursor }
		]

		delegate: MouseArea
		{
			id: handle

			required property var modelData

			readonly property bool onTop: (handle.modelData.edge & Qt.TopEdge) !== 0
			readonly property bool onBottom: (handle.modelData.edge & Qt.BottomEdge) !== 0
			readonly property bool onLeft: (handle.modelData.edge & Qt.LeftEdge) !== 0
			readonly property bool onRight: (handle.modelData.edge & Qt.RightEdge) !== 0
			readonly property bool isCorner:
				(handle.onTop || handle.onBottom) && (handle.onLeft || handle.onRight)

			readonly property int thickness:
				handle.isCorner ? rootHandles.cornerSize : rootHandles.margin

			// Corners are square and pinned to their corner. Edges run the length of the
			// window, inset by the corner size so the corners keep priority.
			width: handle.isCorner
				? handle.thickness
				: (handle.onLeft || handle.onRight
					? handle.thickness
					: rootHandles.width - rootHandles.cornerSize * 2)

			height: handle.isCorner
				? handle.thickness
				: (handle.onTop || handle.onBottom
					? handle.thickness
					: rootHandles.height - rootHandles.cornerSize * 2)

			x: handle.onLeft
				? 0
				: (handle.onRight ? rootHandles.width - handle.width : rootHandles.cornerSize)

			y: handle.onTop
				? 0
				: (handle.onBottom ? rootHandles.height - handle.height : rootHandles.cornerSize)

			cursorShape: handle.modelData.cursor

			// A maximised window has no edges to drag.
			enabled: Window.window !== null
				&& Window.window.visibility !== Window.Maximized
				&& Window.window.visibility !== Window.FullScreen

			onPressed:
				Window.window.startSystemResize(handle.modelData.edge)
		}
	}
}
