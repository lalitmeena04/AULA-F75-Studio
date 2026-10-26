import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    contentHeight: column.implicitHeight + Theme.pagePad * 2
    clip: true
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        x: Theme.pagePad
        y: Theme.pagePad
        width: page.width - Theme.pagePad * 2
        spacing: 20

        StatusBanner { }

        RowLayout {
            Layout.fillWidth: true
            spacing: 20
            Card {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                title: "Config block (0x80 bytes, command 0x04)"
                iconName: "grid"
                Text {
                    Layout.fillWidth: true
                    text: device.current.configHex !== undefined ? device.current.configHex : "Not read yet. Connect over USB."
                    color: Theme.text; font.pixelSize: 11; font.family: Theme.monoFamily; wrapMode: Text.Wrap
                }
                Text { visible: device.current.configHex !== undefined; text: "[0x09] custom=" + device.current.customMode + "  [0x0a] effect hw=" + device.current.effectHardwareId + " (" + device.current.effectName + ")  [0x16] sleep hw=" + device.current.sleepHardware + "  read " + device.current.readAt; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.monoFamily; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
            Card {
                Layout.preferredWidth: 380
                Layout.alignment: Qt.AlignTop
                title: "About"
                iconName: "info"
                Text { text: "AULA F75 Studio " + appInfo.appVersion + " · Qt " + appInfo.qtVersion; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily }
                Text { text: "Icons: " + (Icons.activeSet === "fluent" ? Icons.fluentFamily + " (Fluent)" : "Font Awesome 6 Free via QtAwesome (fallback)"); color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { text: "Profiles: " + appInfo.storagePath; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.monoFamily; wrapMode: Text.WrapAnywhere; Layout.fillWidth: true }
                Text { Layout.fillWidth: true; text: "Lighting, sleep and reset writes were verified on hardware earlier; key matrix, macro and per-key colour uploads use the same buffer format the keyboard returns on read but are unverified until tested over USB. Every write is read back."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
            }
        }

        Card {
            id: connections
            Layout.fillWidth: true
            title: "Connections"
            iconName: "plug"
            property var interfaces: device.hidInterfaces()
            function rescan() { deviceWatcher.rescan(); interfaces = device.hidInterfaces() }
            Connections { target: deviceWatcher; function onChanged() { connections.interfaces = device.hidInterfaces() } }
            RowLayout {
                Layout.fillWidth: true
                Chip { text: deviceWatcher.usbPresent ? "USB cable connected" : "USB cable absent"; iconName: "usb"; tint: deviceWatcher.usbPresent ? Theme.ok : Theme.textMuted; outlined: !deviceWatcher.usbPresent }
                Chip { text: deviceWatcher.receiverPresent ? "2.4 GHz receiver plugged in" : "Receiver absent"; iconName: "wireless"; tint: deviceWatcher.receiverPresent ? Theme.ok : Theme.textMuted; outlined: !deviceWatcher.receiverPresent }
                Item { Layout.fillWidth: true }
                AccentButton { text: "Rescan"; iconName: "refresh"; primary: false; onClicked: connections.rescan() }
            }
            Text { Layout.fillWidth: true; text: "The list refreshes on its own when a device is plugged in or removed. A fresh USB link is probed automatically (read-only)."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
            RowLayout {
                Layout.fillWidth: true
                visible: connections.interfaces.length > 0
                spacing: 14
                Text { text: "LINK"; Layout.preferredWidth: 64; color: Theme.textFaint; font.pixelSize: 10; font.family: Theme.fontFamily }
                Text { text: "ROLE"; Layout.preferredWidth: 120; color: Theme.textFaint; font.pixelSize: 10; font.family: Theme.fontFamily }
                Text { text: "VID:PID"; Layout.preferredWidth: 80; color: Theme.textFaint; font.pixelSize: 10; font.family: Theme.fontFamily }
                Text { text: "USAGE PAGE / USAGE"; Layout.preferredWidth: 130; color: Theme.textFaint; font.pixelSize: 10; font.family: Theme.fontFamily }
                Text { text: "FEATURE / IN / OUT"; Layout.preferredWidth: 130; color: Theme.textFaint; font.pixelSize: 10; font.family: Theme.fontFamily }
                Text { text: "PATH"; Layout.fillWidth: true; color: Theme.textFaint; font.pixelSize: 10; font.family: Theme.fontFamily }
            }
            Repeater {
                model: connections.interfaces
                delegate: RowLayout {
                    id: row
                    required property var modelData
                    readonly property bool primary: modelData.role === "USB command" || modelData.role === "Receiver status"
                    Layout.fillWidth: true
                    spacing: 14
                    Text { text: row.modelData.link === "usb" ? "USB" : "Receiver"; Layout.preferredWidth: 64; color: Theme.text; font.pixelSize: 11; font.family: Theme.monoFamily }
                    Text { text: row.modelData.role; Layout.preferredWidth: 120; color: row.primary ? Theme.accent : Theme.text; font.pixelSize: 11; font.family: Theme.monoFamily }
                    Text { text: row.modelData.vid + ":" + row.modelData.pid; Layout.preferredWidth: 80; color: Theme.text; font.pixelSize: 11; font.family: Theme.monoFamily }
                    Text { text: row.modelData.usagePage + " / " + row.modelData.usage; Layout.preferredWidth: 130; color: Theme.text; font.pixelSize: 11; font.family: Theme.monoFamily }
                    Text { text: row.modelData.feature + " / " + row.modelData.input + " / " + row.modelData.output; Layout.preferredWidth: 130; color: Theme.text; font.pixelSize: 11; font.family: Theme.monoFamily }
                    Text { text: row.modelData.path; Layout.fillWidth: true; color: Theme.textMuted; font.pixelSize: 11; font.family: Theme.monoFamily; elide: Text.ElideMiddle }
                }
            }
            Text { visible: connections.interfaces.length === 0; text: "No keyboard or receiver interfaces found."; color: Theme.textFaint; font.pixelSize: 12; font.family: Theme.fontFamily }
        }

        Card {
            Layout.fillWidth: true
            title: "HID exchange log"
            iconName: "log"
            Repeater {
                model: device.log
                delegate: Text { required property var modelData; text: modelData; color: modelData.startsWith("←") ? Theme.ok : modelData.startsWith("→") ? Theme.accent : Theme.textMuted; font.pixelSize: 11; font.family: Theme.monoFamily; wrapMode: Text.Wrap; Layout.fillWidth: true }
            }
            Text { visible: device.log.length === 0; text: "No exchanges yet."; color: Theme.textFaint; font.pixelSize: 12; font.family: Theme.fontFamily }
        }
    }
}
