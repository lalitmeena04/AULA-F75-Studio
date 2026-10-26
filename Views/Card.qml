import QtQuick
import QtQuick.Layouts

Rectangle {
    id: card
    default property alias content: column.data
    property int padding: 22
    property color tint: "transparent"
    property int spacing: 14
    property string title: ""
    property string iconName: ""

    color: tint === "transparent" ? Theme.surface : Qt.alpha(tint, 0.08)
    radius: Theme.radius
    border.width: 1
    border.color: tint === "transparent" ? Theme.border : Qt.alpha(tint, 0.35)
    implicitHeight: column.implicitHeight + padding * 2 + (header.visible ? header.height + 10 : 0)
    implicitWidth: column.implicitWidth + padding * 2

    RowLayout {
        id: header
        visible: card.title !== ""
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: card.padding
        anchors.bottomMargin: 0
        height: visible ? 24 : 0
        spacing: 10
        Icon { visible: card.iconName !== ""; name: card.iconName === "" ? "info" : card.iconName; size: 16; color: Theme.accent }
        Text { text: card.title; color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; font.family: Theme.fontFamily; Layout.fillWidth: true }
    }

    ColumnLayout {
        id: column
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.visible ? header.bottom : parent.top
        anchors.margins: card.padding
        anchors.topMargin: header.visible ? 10 : card.padding
        spacing: card.spacing
    }
}
