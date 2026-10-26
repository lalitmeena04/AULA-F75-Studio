import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: bar
    required property var device
    required property var profileStore
    property string title: ""
    height: Theme.topBarHeight
    color: Theme.bg
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.border }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.pagePad
        anchors.rightMargin: 20
        spacing: 14

        Text { text: bar.title; color: Theme.text; font.pixelSize: 20; font.weight: Font.DemiBold; font.family: Theme.fontFamily }
        Item { Layout.fillWidth: true }

        Chip {
            text: profileStore.profiles[profileStore.activeIndex] ? profileStore.profiles[profileStore.activeIndex].name : "Profile"
            iconName: "profiles"
            tint: Theme.textMuted
            outlined: true
        }
        Chip {
            readonly property bool usb: device.link === "usb"
            readonly property bool rx: device.link === "receiver"
            text: device.busy ? "Busy" : device.verified ? "USB · verified" : rx ? "2.4 GHz receiver" : usb ? "USB" : "Disconnected"
            iconName: device.busy ? "refresh" : usb ? "usb" : rx ? "wireless" : "error"
            tint: device.busy ? Theme.warn : device.verified ? Theme.ok : rx ? Theme.warn : Theme.danger
        }
        IconButton {
            icon_: device.busy ? "refresh" : "connect"
            tip: "Connect / re-probe"
            accent: !device.verified
            enabled: !device.busy
            onClicked: device.probe()
        }
    }
}
