import QtQuick

Rectangle {
    id: chip
    property string text: ""
    property color tint: Theme.accent
    property string iconName: ""
    property bool outlined: false
    implicitHeight: 24
    implicitWidth: row.implicitWidth + 20
    radius: 12
    color: outlined ? "transparent" : Qt.alpha(tint, 0.16)
    border.width: 1
    border.color: Qt.alpha(tint, outlined ? 0.6 : 0.3)
    Row {
        id: row
        anchors.centerIn: parent
        spacing: 6
        Icon { visible: chip.iconName !== ""; name: chip.iconName === "" ? "info" : chip.iconName; size: 12; color: chip.tint; anchors.verticalCenter: parent.verticalCenter }
        Text { text: chip.text; color: chip.tint; font.pixelSize: 11; font.weight: Font.DemiBold; font.family: Theme.fontFamily; anchors.verticalCenter: parent.verticalCenter }
    }
}
