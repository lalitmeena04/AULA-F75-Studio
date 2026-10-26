import QtQuick
import QtQuick.Controls

Button {
    id: control
    property bool primary: true
    property bool danger: false
    property string iconName: ""
    implicitHeight: 38
    leftPadding: 18
    rightPadding: 18
    hoverEnabled: true
    font.pixelSize: 13
    font.weight: Font.DemiBold
    font.family: Theme.fontFamily

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.danger ? (control.down ? Qt.darker(Theme.danger, 1.3) : Qt.alpha(Theme.danger, control.hovered ? 0.9 : 0.75))
             : control.primary ? (control.down ? Theme.accentDeep : control.hovered ? Qt.lighter(Theme.accent, 1.08) : Theme.accent)
             : (control.down ? Theme.surfaceHover : control.hovered ? Theme.surfaceRaised : Theme.surface)
        border.width: control.primary || control.danger ? 0 : 1
        border.color: Theme.borderStrong
        opacity: control.enabled ? 1 : 0.4
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    contentItem: Row {
        spacing: 8
        Icon {
            visible: control.iconName !== ""
            name: control.iconName === "" ? "info" : control.iconName
            size: 15
            anchors.verticalCenter: parent.verticalCenter
            color: control.primary || control.danger ? "#061018" : Theme.text
        }
        Text {
            text: control.text
            font: control.font
            color: control.primary || control.danger ? "#061018" : Theme.text
            anchors.verticalCenter: parent.verticalCenter
        }
    }
}
