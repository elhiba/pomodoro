// The chart bars are a nested component and reach rootStats.
pragma ComponentBehavior: Bound

import QtQuick
import QtQuick.Controls

import Pomodoro

// Slide in drawer from the left, mirroring the settings drawer on the right. Deliberately
// a drawer rather than a section of the window: it keeps the main layout untouched.
Item
{
	id: rootStats

	property bool open: false
	property color themeColor: "#12130F"

	required property SessionLog log

	Rectangle
	{
		id: scrim

		anchors.fill: parent
		color: "black"

		opacity: rootStats.open ? 0.4 : 0.0
		visible: scrim.opacity > 0

		Behavior on opacity
		{
			NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
		}

		MouseArea
		{
			anchors.fill: parent

			onClicked:
				rootStats.open = false
		}
	}

	Rectangle
	{
		id: drawer

		width: Math.min(360, rootStats.width * 0.9)
		height: rootStats.height

		// Parked just past the left edge when closed.
		x: rootStats.open ? 0 : -width

		Behavior on x
		{
			NumberAnimation { duration: 250; easing.type: Easing.OutCubic }
		}

		color: Qt.darker(rootStats.themeColor, 1.4)

		Behavior on color
		{
			ColorAnimation { duration: 500; easing.type: Easing.InOutQuad }
		}

		MouseArea
		{
			anchors.fill: parent
		}

		Item
		{
			id: header

			anchors.top: parent.top
			anchors.left: parent.left
			anchors.right: parent.right

			height: 56

			Text
			{
				anchors.left: parent.left
				anchors.leftMargin: 20
				anchors.verticalCenter: parent.verticalCenter

				text: "Statistics"
				color: "white"
				font.pixelSize: 20
				font.bold: true
			}

			Button
			{
				id: closeStatsBtn

				width: 36
				height: 36

				anchors.right: parent.right
				anchors.rightMargin: 12
				anchors.verticalCenter: parent.verticalCenter

				background: Rectangle
				{
					radius: 6
					color: closeStatsBtn.hovered ? Qt.rgba(1, 1, 1, 0.18) : "transparent"
				}

				icon.source: "assets/icons/close.svg"
				icon.color: "white"
				icon.width: 16
				icon.height: 16

				onClicked:
					rootStats.open = false
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

		ScrollView
		{
			id: body

			anchors.top: header.bottom
			anchors.bottom: parent.bottom
			anchors.left: parent.left
			anchors.right: parent.right
			anchors.topMargin: 20
			anchors.bottomMargin: 20
			anchors.leftMargin: 20
			anchors.rightMargin: 4

			// The text keeps its 20 pixel margin; the bar lives in the gap beside it
			// instead of on top of it.
			rightPadding: 16

			clip: true
			contentWidth: availableWidth
			ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
			ScrollBar.vertical: SlimScrollBar
			{
				parent: body
				x: body.width - width
				y: body.topPadding
				height: body.availableHeight
			}

			Column
			{
				width: body.availableWidth
				spacing: 16

				// --- the headline numbers ---
				Grid
				{
					width: parent.width

					columns: 2
					columnSpacing: 12
					rowSpacing: 12

					Repeater
					{
						model: [
							{ caption: "Focus today", value: rootStats.log.todayFocusMinutes + " min" },
							{ caption: "Pomodoros today", value: String(rootStats.log.todayPomodoros) },
							{ caption: "Last 7 days", value: rootStats.log.weekFocusMinutes + " min" },
							{ caption: "Day streak", value: String(rootStats.log.currentStreak) }
						]

						delegate: Rectangle
						{
							id: tile

							required property var modelData

							width: (body.availableWidth - 12) / 2
							height: 72
							radius: 10

							color: Qt.rgba(1, 1, 1, 0.1)
							border.color: Qt.rgba(1, 1, 1, 0.18)
							border.width: 1

							Column
							{
								anchors.centerIn: parent
								spacing: 4

								Text
								{
									anchors.horizontalCenter: parent.horizontalCenter
									text: tile.modelData.value
									color: "white"
									font.pixelSize: 22
									font.bold: true
								}

								Text
								{
									anchors.horizontalCenter: parent.horizontalCenter
									text: tile.modelData.caption
									color: Qt.rgba(1, 1, 1, 0.6)
									font.pixelSize: 11
								}
							}
						}
					}
				}

				Text
				{
					text: "LAST 7 DAYS"
					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 11
					font.bold: true
					font.letterSpacing: 1.2
					topPadding: 8
				}

				// --- the chart: seven plain rectangles, no charting dependency ---
				Row
				{
					id: chart

					width: parent.width
					height: 150

					property int columnWidth: (width - spacing * 6) / 7
					property int peak: Math.max(rootStats.log.recentPeakMinutes, 1)

					spacing: 8

					Repeater
					{
						model: rootStats.log.recentDays

						delegate: Item
						{
							id: bar

							required property var modelData

							width: chart.columnWidth
							height: chart.height

							Text
							{
								id: barValue

								anchors.horizontalCenter: parent.horizontalCenter
								anchors.bottom: barShape.top
								anchors.bottomMargin: 4

								text: String(bar.modelData.minutes)
								color: Qt.rgba(1, 1, 1, 0.7)
								font.pixelSize: 10

								visible: bar.modelData.minutes > 0
							}

							Rectangle
							{
								id: barShape

								width: parent.width
								anchors.bottom: dayLabel.top
								anchors.bottomMargin: 6

								// Always a sliver, so an empty day still reads as a column.
								height: Math.max(3, (parent.height - 40) * bar.modelData.minutes / chart.peak)

								radius: 4

								color: bar.modelData.isToday ? "white" : Qt.rgba(1, 1, 1, 0.35)

								Behavior on height
								{
									NumberAnimation { duration: 250; easing.type: Easing.OutQuad }
								}
							}

							Text
							{
								id: dayLabel

								anchors.horizontalCenter: parent.horizontalCenter
								anchors.bottom: parent.bottom

								text: bar.modelData.label
								color: bar.modelData.isToday ? "white" : Qt.rgba(1, 1, 1, 0.55)
								font.pixelSize: 11
								font.bold: bar.modelData.isToday
							}
						}
					}
				}

				Rectangle
				{
					width: parent.width
					height: 1
					color: Qt.rgba(1, 1, 1, 0.15)
				}

				Text
				{
					width: parent.width

					text: rootStats.log.totalPomodoros === 0
						? "No sessions finished yet. Numbers appear here once a focus session runs all the way out."
						: rootStats.log.totalPomodoros + " focus sessions finished, all time."

					color: Qt.rgba(1, 1, 1, 0.6)
					font.pixelSize: 12
					wrapMode: Text.WordWrap
				}

				// Two step, because there is no undo for this.
				Button
				{
					id: clearBtn

					property bool armed: false

					width: parent.width
					height: 40

					visible: rootStats.log.totalPomodoros > 0

					background: Rectangle
					{
						radius: 8
						color: clearBtn.armed
							? "#d91629"
							: clearBtn.hovered ? Qt.rgba(1, 1, 1, 0.2) : Qt.rgba(1, 1, 1, 0.1)

						border.color: Qt.rgba(1, 1, 1, 0.25)
						border.width: 1

						Behavior on color
						{
							ColorAnimation { duration: 150 }
						}
					}

					contentItem: Text
					{
						text: clearBtn.armed ? "Really clear everything?" : "Clear history"
						color: "white"
						font.pixelSize: 14

						horizontalAlignment: Text.AlignHCenter
						verticalAlignment: Text.AlignVCenter
					}

					onClicked:
					{
						if (clearBtn.armed)
						{
							rootStats.log.clearHistory()
							clearBtn.armed = false
						}
						else
						{
							clearBtn.armed = true
							disarm.restart()
						}
					}

					Timer
					{
						id: disarm
						interval: 4000

						onTriggered:
							clearBtn.armed = false
					}
				}
			}
		}
	}

	// Reaching for the stats while it is armed should not leave it armed next time.
	onOpenChanged:
		if (!rootStats.open)
			clearBtn.armed = false
}
