import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ColumnLayout {
    id: control
    property string label: ""
    property string iconName: ""
    property alias from: slider.from
    property alias to: slider.to
    property alias value: slider.value
    property alias stepSize: slider.stepSize
    property string valueText: Math.round(slider.value).toString()
    property bool live: false
    signal committed(real value)
    spacing: 6

    RowLayout {
        Layout.fillWidth: true
        Icon { visible: control.iconName !== ""; name: control.iconName === "" ? "info" : control.iconName; size: 14 }
        Text { text: control.label; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; Layout.fillWidth: true }
        Text { text: control.valueText; color: Theme.text; font.pixelSize: 12; font.weight: Font.DemiBold; font.family: Theme.fontFamily }
    }
    Slider {
        id: slider
        Layout.fillWidth: true
        stepSize: 1
        snapMode: Slider.SnapAlways
        onMoved: if (control.live) control.committed(value)
        onPressedChanged: if (!pressed) control.committed(value)
        background: Rectangle {
            x: slider.leftPadding
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: slider.availableWidth
            height: 4
            radius: 2
            color: Theme.border
            Rectangle { width: slider.visualPosition * parent.width; height: parent.height; radius: 2; color: Theme.accent }
        }
        handle: Rectangle {
            x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
            y: slider.topPadding + slider.availableHeight / 2 - height / 2
            width: 16; height: 16; radius: 8
            color: slider.pressed ? Qt.lighter(Theme.accent, 1.2) : Theme.text
            border.width: 2
            border.color: Theme.accent
        }
    }
}
