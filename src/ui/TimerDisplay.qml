import QtQuick
import QtQuick.Controls
import QtQuick.Effects

Item
{
    id: rootTimer
	anchors.fill: parent

	signal startClicked()

	FontLoader
	{
        id: timerFont
        source: "assets/fonts/JetBrainsMono.ttf" 
    }

	Rectangle
	{
		id: glassPanel
        width: mainWindow.width * 0.5
        height: mainWindow.height * 0.5
        anchors.centerIn: parent
        
        // The Glass Recipe: 15% opaque white, smooth corners, 30% opaque border
        color: Qt.rgba(1, 1, 1, 0.15)
        radius: 24
        border.color: Qt.rgba(1, 1, 1, 0.3)
        border.width: 1

		Column
		{
			anchors.centerIn: parent
			spacing: 30

			Text
			{
				id: timeText
				text: "25:00"
				color: "white"
				font.family: timerFont.name
				font.pixelSize: glassPanel.width * 0.2

				horizontalAlignment: Text.AlignHCenter
				anchors.horizontalCenter: parent.horizontalCenter
			}

			Button
			{
				id: startBtn
				width: glassPanel.width * 0.25
				height: glassPanel.height * 0.25

				anchors.horizontalCenter: parent.horizontalCenter

				property int depth: 6 

				background: Item
				{
					Rectangle
					{
						anchors.fill: parent
						anchors.topMargin: startBtn.depth 
						color: "#e0e0e0"
						radius: 8
					}

					Rectangle
					{
						width: parent.width
						height: parent.height - startBtn.depth

						color: "white"
						radius: 8

						y: startBtn.pressed ? startBtn.depth : 0

						Behavior on y {
							NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
						}
					}
				}

				contentItem: Item
				{
					Text
					{
						text: "START"
						font.pixelSize: startBtn.width * 0.2
						font.bold: true
						color: themeColor

						anchors.horizontalCenter: parent.horizontalCenter
						anchors.verticalCenter: parent.verticalCenter

						anchors.verticalCenterOffset: startBtn.pressed ? startBtn.depth : 0

						Behavior on anchors.verticalCenterOffset {
							NumberAnimation { duration: 80; easing.type: Easing.OutQuad }
						}
					}
				}

				onClicked:
				{
					rootTimer.startClicked()
					fakeTimer.start()
				}

			}
	}
		PropertyAnimation
		{
    	    id: fakeTimer
    	    target: mainWindow
    	    property: "timerProgress"
    	    to: 0.0
    	    duration: 10000 // 10 seconds
    	}
	}
}
