import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Hue/saturation/value picker with swatches; emits a #rrggbb colour.
ColumnLayout {
    id: control
    property color color: "#2ec4ff"
    signal picked(color value)
    spacing: 10

    function setHsv(h, s, v) {
        control.color = Qt.hsva(h, s, v, 1)
        control.picked(control.color)
    }

    RowLayout {
        spacing: 10
        Rectangle { width: 44; height: 44; radius: 10; color: control.color; border.width: 1; border.color: Theme.borderStrong }
        ColumnLayout {
            spacing: 2
            Text { text: control.color.toString().toUpperCase(); color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold; font.family: Theme.monoFamily }
            Text { text: "H " + Math.round(control.color.hsvHue * 360) + "°  S " + Math.round(control.color.hsvSaturation * 100) + "%  V " + Math.round(control.color.hsvValue * 100) + "%"; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
        }
    }
    // Hue strip
    Rectangle {
        Layout.fillWidth: true
        height: 18
        radius: 9
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#ff0000" }
            GradientStop { position: 0.17; color: "#ffff00" }
            GradientStop { position: 0.33; color: "#00ff00" }
            GradientStop { position: 0.5; color: "#00ffff" }
            GradientStop { position: 0.67; color: "#0000ff" }
            GradientStop { position: 0.83; color: "#ff00ff" }
            GradientStop { position: 1.0; color: "#ff0000" }
        }
        Rectangle {
            x: Math.max(0, Math.min(parent.width - width, control.color.hsvHue * parent.width - width / 2))
            width: 6; height: parent.height + 6; y: -3; radius: 3
            color: "white"; border.width: 1; border.color: "#000"
        }
        MouseArea {
            anchors.fill: parent
            function pick(mx) { control.setHsv(Math.max(0, Math.min(1, mx / width)), Math.max(0.05, control.color.hsvSaturation), Math.max(0.05, control.color.hsvValue)) }
            onPressed: (mouse) => pick(mouse.x)
            onPositionChanged: (mouse) => { if (pressed) pick(mouse.x) }
        }
    }
    LabeledSlider {
        Layout.fillWidth: true; label: "Saturation"; from: 0; to: 100; live: true
        value: Math.round(control.color.hsvSaturation * 100)
        onCommitted: (v) => control.setHsv(control.color.hsvHue < 0 ? 0 : control.color.hsvHue, v / 100, control.color.hsvValue)
    }
    LabeledSlider {
        Layout.fillWidth: true; label: "Brightness"; from: 0; to: 100; live: true
        value: Math.round(control.color.hsvValue * 100)
        onCommitted: (v) => control.setHsv(control.color.hsvHue < 0 ? 0 : control.color.hsvHue, control.color.hsvSaturation, v / 100)
    }
    Flow {
        Layout.fillWidth: true
        spacing: 6
        Repeater {
            model: ["#ff2d55", "#ff9500", "#ffd60a", "#34c759", "#00e5c3", "#2ec4ff", "#5e5ce6", "#bf5af2", "#ffffff", "#000000"]
            delegate: Rectangle {
                required property string modelData
                width: 24; height: 24; radius: 6; color: modelData
                border.width: 1; border.color: Theme.borderStrong
                MouseArea { anchors.fill: parent; onClicked: { control.color = parent.modelData; control.picked(control.color) } }
            }
        }
    }
}
