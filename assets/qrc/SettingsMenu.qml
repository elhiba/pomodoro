import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    color: "#000000"

    signal closeRequested()

    ScrollView {
        id: settingsScroll // 1. Give the ScrollView an ID
        anchors.fill: parent
        anchors.margins: 20
        clip: true
        
        // 2. Lock the content width and disable horizontal scrolling
        contentWidth: availableWidth 
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff 

        ColumnLayout {
            // 3. Bind to availableWidth instead of parent.width
            width: settingsScroll.availableWidth 
            spacing: 20

            // --- HEADER ---
            RowLayout {
                Layout.fillWidth: true

                Text {
                    text: "Settings"
                    color: "white"
                    font.pixelSize: 24
                    font.bold: true
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#444" }

            // --- 1. TIMER SETTINGS ---
            Text { text: "Work Duration"; color: "#aaa"; font.pixelSize: 14 }
            RowLayout {
                spacing: 10
                SpinBox {
                    id: minBox
                    Layout.preferredWidth: 60
                    from: 0
                    to: 120
                    value: 25
                    editable: true
                    onValueChanged: if (typeof timerLogic !== "undefined") timerLogic.setTimerDuration(value, secBox.value)
                }
                Text { text: "min"; color: "white" }

                SpinBox {
                    id: secBox
                    Layout.preferredWidth: 60
                    from: 0
                    to: 59
                    value: 0
                    editable: true
                    onValueChanged: if (typeof timerLogic !== "undefined") timerLogic.setTimerDuration(minBox.value, value)
                }
                Text { text: "sec"; color: "white" }
            }

            // --- 2. AUDIO STREAM URL ---
            Text { text: "Lo-fi Stream URL"; color: "#aaa"; font.pixelSize: 14; Layout.topMargin: 10 }
            TextField {
                id: streamUrlInput
                Layout.fillWidth: true
                clip: true // 4. Add clip to ensure text stays inside the box boundaries
                
                text: "https://stream.zeno.fm/f3wvbbqmdg8uv"
                placeholderText: "Enter audio stream URL..."
                color: "white"
                background: Rectangle { color: "#1e1e1e"; border.color: "#444"; radius: 4 }

                onTextChanged: if (typeof timerLogic !== "undefined") timerLogic.setStreamUrl(text)
            }

            // --- 3. VOLUME ---
            Text { text: "Stream Volume"; color: "#aaa"; font.pixelSize: 14; Layout.topMargin: 10 }
            Slider {
                id: volumeSlider
                Layout.fillWidth: true
                from: 0.0
                to: 1.0
                value: 0.5
                onValueChanged: {
                    if (typeof timerLogic !== "undefined") {
                        timerLogic.setVolume(value)
                    }
                }
            }

            Rectangle { Layout.fillWidth: true; height: 1; color: "#444"; Layout.topMargin: 10 } 

            // --- 4. TOGGLES ---
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Auto-start Timer"; color: "white"; font.pixelSize: 16; Layout.fillWidth: true }
                Switch { 
                    id: autoTimerSwitch 
                    checked: false 
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Text { text: "Auto-start Music"; color: "white"; font.pixelSize: 16; Layout.fillWidth: true }
                Switch { 
                    id: autoMusicSwitch 
                    checked: true 
                    onCheckedChanged: if (typeof timerLogic !== "undefined") timerLogic.setAutoStartMusic(checked)
                }
            }
        }
    }
}
