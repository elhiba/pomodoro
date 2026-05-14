import QtQuick
import QtQuick.Shapes

Rectangle {
    id: root
    width: 500
    height: 500
    color: "#12130F"

    property real progress: 1
    property string timeText: "25:00"
    
    property real centerPoint: width / 2
    property real ringRadius: (width / 2) - 30 
    
    property int fgRingThickness: 26 
    property int bgRingThickness: 22 

	FontLoader
	{
		id: timerFont
		source: "qrc:/JetBrainsMonoFont"
	}

    Rectangle {
        anchors.centerIn: parent
        width: (root.ringRadius * 2) + root.bgRingThickness
        height: width
        radius: width / 2
        
        color: "transparent"
        border.color: "#FFFFFF" 
        border.width: root.bgRingThickness
        
        antialiasing: true 
    }

    Shape {
        anchors.fill: parent
        antialiasing: true
        preferredRendererType: Shape.CurveRenderer
        layer.enabled: false 

        ShapePath {
            fillColor: "transparent"
            strokeColor: "#619b80"
            strokeWidth: root.fgRingThickness
            
            capStyle: ShapePath.FlatCap 

            PathAngleArc {
                centerX: root.centerPoint
                centerY: root.centerPoint
                radiusX: root.ringRadius
                radiusY: root.ringRadius
                
                startAngle: -90
                sweepAngle: root.progress * 360
            }
        }
    }

    Text {
        anchors.centerIn: parent
        text: root.timeText
        color: "white"
        font.pixelSize: 50
        font.bold: true

		font.family: timerFont.name
    }
}
