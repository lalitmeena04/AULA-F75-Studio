import QtQuick
import QtQuick.Layouts

// Inline status strip explaining what the device said last.
Rectangle {
    id: banner
    readonly property bool bad: device.state === "error" || device.state === "mismatch" || device.state === "absent"
    readonly property bool warnish: device.state === "receiver" || device.state === "idle"
    readonly property color tint: bad ? Theme.danger : warnish ? Theme.warn : device.state === "busy" ? Theme.accent : Theme.ok
    Layout.fillWidth: true
    implicitHeight: row.implicitHeight + 20
    radius: Theme.radiusSmall
    color: Qt.alpha(tint, 0.08)
    border.width: 1
    border.color: Qt.alpha(tint, 0.3)
    RowLayout {
        id: row
        anchors.fill: parent
        anchors.margins: 10
        anchors.leftMargin: 14
        spacing: 12
        Icon { name: banner.bad ? "error" : banner.warnish ? "warning" : device.state === "busy" ? "refresh" : "ok"; size: 16; color: banner.tint }
        Text {
            Layout.fillWidth: true
            text: device.message
            color: Theme.text
            font.pixelSize: 12
            font.family: Theme.fontFamily
            wrapMode: Text.WordWrap
        }
    }
}
