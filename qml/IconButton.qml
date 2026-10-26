import QtQuick
import QtQuick.Controls

AbstractButton {
    id: control
    required property string icon_
    property int size: 36
    property int glyphSize: 18
    property bool accent: false
    property bool danger: false
    property string tip: ""

    implicitWidth: size
    implicitHeight: size
    hoverEnabled: true
    ToolTip.visible: tip !== "" && hovered
    ToolTip.text: tip
    ToolTip.delay: 500

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.down ? Theme.surfaceHover
             : control.hovered ? Theme.surfaceRaised
             : control.accent ? Qt.alpha(Theme.accent, 0.14) : "transparent"
        border.width: control.accent ? 1 : 0
        border.color: Qt.alpha(Theme.accent, 0.45)
        opacity: control.enabled ? 1 : 0.4
        Behavior on color { ColorAnimation { duration: 120 } }
    }
    contentItem: Icon {
        name: control.icon_
        size: control.glyphSize
        color: control.danger ? Theme.danger : control.accent ? Theme.accent : control.hovered ? Theme.text : Theme.textMuted
    }
}
