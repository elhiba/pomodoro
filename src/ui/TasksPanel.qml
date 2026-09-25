// The delegate reaches rootPanel and the list's id from inside a nested component.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import Pomodoro

// The to-do list, in a drawer from the right -- the other two come in from the left, and
// the button that opens this one sits on the right of the title bar.
//
// Type and press Enter to add. Click a task to work on it: the active task collects the
// focus sessions that finish while it is picked, and shows under the timer. The circle
// marks it done; hovering shows the estimate controls and the delete button. The arrow
// opens the task's description and its checklist of steps.
Item
{
	id: rootPanel

	required property TaskList tasks

	property bool open: false
	property color themeColor: "#12130F"

	// Single letter shortcuts have to stand down while a task is being typed -- in any of
	// the drawer's fields, the descriptions and steps included.
	readonly property bool typing: newTaskField.activeFocus || taskList.editing
		|| (rootPanel.open && (rootPanel.Window.activeFocusItem instanceof TextInput
			|| rootPanel.Window.activeFocusItem instanceof TextEdit))

	Rectangle
	{
		id: scrim

		anchors.fill: parent
		color: "black"

		opacity: rootPanel.open ? 0.4 : 0.0
		visible: scrim.opacity > 0

		Behavior on opacity
		{
			NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
		}

		MouseArea
		{
			anchors.fill: parent

			onClicked:
				rootPanel.open = false
		}
	}

	Rectangle
	{
		id: drawer

		width: Math.min(380, rootPanel.width * 0.9)
		height: rootPanel.height

		// Parked just past the right edge when closed.
		x: rootPanel.open ? rootPanel.width - width : rootPanel.width

		Behavior on x
		{
			NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
		}

		color: Qt.darker(rootPanel.themeColor, 1.4)

		Behavior on color
		{
			ColorAnimation { duration: 500; easing.type: Easing.InOutQuad }
		}

		// The drawer eats clicks so they do not reach the scrim underneath.
		MouseArea
		{
			anchors.fill: parent
		}

		Rectangle
		{
			id: header

			anchors.top: parent.top
			anchors.left: parent.left
			anchors.right: parent.right

			height: 56
			color: "transparent"

			Text
			{
				anchors.left: parent.left
				anchors.leftMargin: 20
				anchors.verticalCenter: parent.verticalCenter

				text: "Tasks"
				color: "white"
				font.pixelSize: 20
				font.bold: true
			}

			Button
			{
				id: closePanelBtn

				width: 36
				height: 36

				anchors.right: parent.right
				anchors.rightMargin: 12
				anchors.verticalCenter: parent.verticalCenter

				background: Rectangle
				{
					radius: 6
					color: closePanelBtn.hovered ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
				}

				icon.source: "assets/icons/close.svg"
				icon.color: "white"
				icon.width: 16
				icon.height: 16

				onClicked:
					rootPanel.open = false
			}

			Rectangle
			{
				anchors.bottom: parent.bottom
				anchors.left: parent.left
				anchors.right: parent.right

				height: 1
				color: Qt.rgba(1, 1, 1, 0.15)
			}
		}

		TextField
		{
			id: newTaskField

			anchors.top: header.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.margins: 20

			height: 40

			placeholderText: "Add a task and press Enter"
			placeholderTextColor: Qt.rgba(1, 1, 1, 0.45)
			color: "white"
			font.pixelSize: 14
			maximumLength: 200

			background: Rectangle
			{
				radius: 8
				color: Qt.rgba(0, 0, 0, 0.2)
				border.color: newTaskField.activeFocus ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.22)
				border.width: 1
			}

			// The field keeps the focus after adding, so a list can be typed in one go.
			onAccepted:
				if (rootPanel.tasks.add(newTaskField.text))
					newTaskField.clear()

			Keys.onEscapePressed:
				rootPanel.open = false
		}

		ListView
		{
			id: taskList

			// True while any row's title is being edited.
			property bool editing: false

			anchors.top: newTaskField.bottom
			anchors.topMargin: 12
			anchors.bottom: footer.top
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.leftMargin: 12
			anchors.rightMargin: 12

			clip: true
			spacing: 4
			boundsBehavior: Flickable.StopAtBounds

			model: rootPanel.tasks

			ScrollBar.vertical: SlimScrollBar {}

			delegate: Rectangle
			{
				id: row

				required property int index
				required property string title
				required property bool done
				required property int estimate
				required property int completed
				required property bool active
				required property string notes
				required property var steps
				required property int stepCount
				required property int stepsDone

				// Open to show the description and the steps.
				property bool expanded: false

				readonly property bool hasDetails: row.notes.length > 0 || row.stepCount > 0

				width: ListView.view.width
				height: headerArea.height + (row.expanded ? details.implicitHeight + 10 : 0)
				radius: 8
				clip: true

				color: row.active
					? Qt.rgba(1, 1, 1, 0.2)
					: rowHover.hovered || row.expanded ? Qt.rgba(1, 1, 1, 0.08) : "transparent"

				border.color: row.active ? Qt.rgba(1, 1, 1, 0.55) : "transparent"
				border.width: 1

				HoverHandler
				{
					id: rowHover
				}

				Item
				{
					id: headerArea

					width: parent.width
					height: 48

					// Clicking the row (anywhere the buttons are not) picks it to work on.
					MouseArea
					{
						anchors.fill: parent

						onClicked:
							rootPanel.tasks.toggleActive(row.index)

						onDoubleClicked:
							titleEdit.begin()
					}

					// The done circle.
					Rectangle
					{
						id: doneMark

						anchors.left: parent.left
						anchors.leftMargin: 10
						anchors.verticalCenter: parent.verticalCenter

						width: 20
						height: 20
						radius: 10

						color: row.done ? "white" : "transparent"
						border.color: "white"
						border.width: 2
						opacity: row.done ? 0.8 : 0.9

						Text
						{
							anchors.centerIn: parent
							visible: row.done
							text: "✓"
							color: rootPanel.themeColor
							font.pixelSize: 13
							font.bold: true
						}

						MouseArea
						{
							anchors.fill: parent
							anchors.margins: -6
							cursorShape: Qt.PointingHandCursor

							onClicked:
							{
								SoundPlayer.playClick()
								rootPanel.tasks.setDone(row.index, !row.done)
							}
						}
					}

					// The title, and under it how far the steps have got.
					Column
					{
						anchors.left: doneMark.right
						anchors.leftMargin: 12
						anchors.right: tally.left
						anchors.rightMargin: 8
						anchors.verticalCenter: parent.verticalCenter

						visible: !titleEdit.visible

						Text
						{
							width: parent.width

							text: row.title
							textFormat: Text.PlainText
							color: "white"
							opacity: row.done ? 0.5 : 1.0
							font.pixelSize: 14
							font.bold: row.active
							font.strikeout: row.done
							elide: Text.ElideRight
						}

						Text
						{
							width: parent.width
							visible: row.stepCount > 0 || row.notes.length > 0

							text: row.stepCount > 0
								? row.stepsDone + " of " + row.stepCount + " steps done"
								: "Has a description"
							textFormat: Text.PlainText
							color: "white"
							opacity: 0.55
							font.pixelSize: 11
							elide: Text.ElideRight
						}
					}

					TextField
					{
						id: titleEdit

						anchors.left: doneMark.right
						anchors.leftMargin: 8
						anchors.right: tally.left
						anchors.rightMargin: 8
						anchors.verticalCenter: parent.verticalCenter

						height: 34
						visible: false

						color: "white"
						font.pixelSize: 14
						maximumLength: 200

						background: Rectangle
						{
							radius: 6
							color: Qt.rgba(0, 0, 0, 0.25)
							border.color: Qt.rgba(1, 1, 1, 0.5)
							border.width: 1
						}

						function begin()
						{
							titleEdit.text = row.title
							titleEdit.visible = true
							taskList.editing = true
							titleEdit.forceActiveFocus()
							titleEdit.selectAll()
						}

						function finish(keep)
						{
							if (!titleEdit.visible)
								return

							titleEdit.visible = false
							taskList.editing = false

							if (keep)
								rootPanel.tasks.rename(row.index, titleEdit.text)
						}

						onAccepted:
							titleEdit.finish(true)

						onActiveFocusChanged:
							if (!titleEdit.activeFocus)
								titleEdit.finish(true)

						Keys.onEscapePressed:
							titleEdit.finish(false)
					}

					// Sessions done out of sessions planned, with the controls to change the
					// plan and to delete the task showing only while the row is hovered. The
					// arrow opens the description and the steps; it stays in view once the
					// task has either, so they are not hidden behind a hover.
					Row
					{
						id: tally

						anchors.right: parent.right
						anchors.rightMargin: 8
						anchors.verticalCenter: parent.verticalCenter

						spacing: 2

						SmallButton
						{
							visible: rowHover.hovered
							label: "−"

							onClicked:
								rootPanel.tasks.setEstimate(row.index, row.estimate - 1)
						}

						Text
						{
							anchors.verticalCenter: parent.verticalCenter

							width: 38
							horizontalAlignment: Text.AlignHCenter

							text: row.completed + "/" + row.estimate
							color: "white"
							opacity: 0.75
							font.pixelSize: 13
						}

						SmallButton
						{
							visible: rowHover.hovered
							label: "+"

							onClicked:
								rootPanel.tasks.setEstimate(row.index, row.estimate + 1)
						}

						SmallButton
						{
							visible: rowHover.hovered
							label: "×"
							danger: true

							onClicked:
								rootPanel.tasks.remove(row.index)
						}

						SmallButton
						{
							id: expandBtn

							visible: rowHover.hovered || row.expanded || row.hasDetails
							label: "›"
							rotation: row.expanded ? 90 : 0

							ToolTip.visible: expandBtn.hovered
							ToolTip.delay: 500
							ToolTip.text: row.expanded ? "Hide the details" : "Description and steps"

							Behavior on rotation
							{
								NumberAnimation { duration: 150 }
							}

							onClicked:
								row.expanded = !row.expanded
						}
					}
				}

				// The description and the checklist, under the row while it is open.
				Column
				{
					id: details

					anchors.top: headerArea.bottom
					anchors.left: parent.left
					anchors.right: parent.right
					anchors.leftMargin: 12
					anchors.rightMargin: 12

					visible: row.expanded
					spacing: 6

					TextArea
					{
						id: notesEdit

						width: parent.width

						text: row.notes
						placeholderText: "Description: what this task is about"
						placeholderTextColor: Qt.rgba(1, 1, 1, 0.4)
						color: "white"
						font.pixelSize: 13
						wrapMode: TextEdit.Wrap
						selectByMouse: true

						background: Rectangle
						{
							radius: 6
							color: Qt.rgba(0, 0, 0, 0.2)
							border.color: notesEdit.activeFocus ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.15)
							border.width: 1
						}

						// Saved when the field is left, not on every key.
						onActiveFocusChanged:
							if (!notesEdit.activeFocus)
								rootPanel.tasks.setNotes(row.index, notesEdit.text)

						Keys.onEscapePressed:
							notesEdit.focus = false
					}

					Repeater
					{
						model: row.steps

						delegate: Item
						{
							id: stepRow

							required property int index
							required property var modelData

							width: details.width
							height: 30

							HoverHandler
							{
								id: stepHover
							}

							Rectangle
							{
								id: stepMark

								anchors.left: parent.left
								anchors.leftMargin: 4
								anchors.verticalCenter: parent.verticalCenter

								width: 16
								height: 16
								radius: 4

								color: stepRow.modelData.done ? "white" : "transparent"
								border.color: "white"
								border.width: 2
								opacity: 0.85

								Text
								{
									anchors.centerIn: parent
									visible: stepRow.modelData.done
									text: "✓"
									color: rootPanel.themeColor
									font.pixelSize: 11
									font.bold: true
								}

								MouseArea
								{
									anchors.fill: parent
									anchors.margins: -6
									cursorShape: Qt.PointingHandCursor

									onClicked:
									{
										SoundPlayer.playClick()
										rootPanel.tasks.setStepDone(row.index, stepRow.index, !stepRow.modelData.done)
									}
								}
							}

							// Edited in place: the frame only shows while it has the focus.
							TextField
							{
								id: stepEdit

								anchors.left: stepMark.right
								anchors.leftMargin: 6
								anchors.right: removeStepBtn.left
								anchors.rightMargin: 4
								anchors.verticalCenter: parent.verticalCenter

								height: 28
								leftPadding: 6
								rightPadding: 6

								text: stepRow.modelData.text
								color: "white"
								opacity: stepRow.modelData.done && !stepEdit.activeFocus ? 0.5 : 1.0
								font.pixelSize: 13
								font.strikeout: stepRow.modelData.done && !stepEdit.activeFocus
								maximumLength: 200
								selectByMouse: true

								background: Rectangle
								{
									radius: 5
									color: stepEdit.activeFocus ? Qt.rgba(0, 0, 0, 0.25) : "transparent"
									border.color: stepEdit.activeFocus ? Qt.rgba(1, 1, 1, 0.4) : "transparent"
									border.width: 1
								}

								onAccepted:
									stepEdit.focus = false

								onActiveFocusChanged:
									if (!stepEdit.activeFocus)
										rootPanel.tasks.renameStep(row.index, stepRow.index, stepEdit.text)

								Keys.onEscapePressed:
								{
									stepEdit.text = stepRow.modelData.text
									stepEdit.focus = false
								}
							}

							SmallButton
							{
								id: removeStepBtn

								anchors.right: parent.right
								anchors.verticalCenter: parent.verticalCenter

								opacity: stepHover.hovered ? 1.0 : 0.0
								label: "×"
								danger: true

								onClicked:
									rootPanel.tasks.removeStep(row.index, stepRow.index)
							}
						}
					}

					TextField
					{
						id: newStepField

						width: parent.width
						height: 32

						placeholderText: "Add a step and press Enter"
						placeholderTextColor: Qt.rgba(1, 1, 1, 0.4)
						color: "white"
						font.pixelSize: 13
						maximumLength: 200

						background: Rectangle
						{
							radius: 6
							color: Qt.rgba(0, 0, 0, 0.2)
							border.color: newStepField.activeFocus ? Qt.rgba(1, 1, 1, 0.5) : Qt.rgba(1, 1, 1, 0.15)
							border.width: 1
						}

						// Keeps the focus, so a whole checklist can be typed in one go.
						onAccepted:
							if (rootPanel.tasks.addStep(row.index, newStepField.text))
								newStepField.clear()

						Keys.onEscapePressed:
							newStepField.focus = false
					}
				}
			}

			// Said rather than left blank, so an empty drawer explains itself.
			Text
			{
				anchors.centerIn: parent
				width: parent.width - 40

				visible: taskList.count === 0

				text: "No tasks yet. Add what you want to get done, then click one to work on it -- every focus session you finish counts towards it."
				color: Qt.rgba(1, 1, 1, 0.55)
				font.pixelSize: 13
				wrapMode: Text.WordWrap
				horizontalAlignment: Text.AlignHCenter
			}
		}

		Item
		{
			id: footer

			anchors.bottom: parent.bottom
			anchors.left: parent.left
			anchors.right: parent.right

			height: clearBtn.visible ? 64 : 12

			Button
			{
				id: clearBtn

				anchors.centerIn: parent

				width: parent.width - 40
				height: 38

				visible: rootPanel.tasks.count > rootPanel.tasks.openCount

				background: Rectangle
				{
					radius: 8
					color: clearBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)
					border.color: Qt.rgba(1, 1, 1, 0.25)
					border.width: 1
				}

				contentItem: Text
				{
					text: "Clear finished tasks"
					color: "white"
					font.pixelSize: 14
					horizontalAlignment: Text.AlignHCenter
					verticalAlignment: Text.AlignVCenter
				}

				onClicked:
					rootPanel.tasks.clearDone()
			}
		}
	}

	component SmallButton: Button
	{
		id: smallBtn

		property string label: ""
		property bool danger: false

		width: 24
		height: 24

		background: Rectangle
		{
			radius: 6
			color: smallBtn.hovered
				? (smallBtn.danger ? "#d91629" : Qt.rgba(1, 1, 1, 0.22))
				: Qt.rgba(1, 1, 1, 0.08)
		}

		contentItem: Text
		{
			text: smallBtn.label
			color: "white"
			font.pixelSize: 15
			horizontalAlignment: Text.AlignHCenter
			verticalAlignment: Text.AlignVCenter
		}
	}
}
