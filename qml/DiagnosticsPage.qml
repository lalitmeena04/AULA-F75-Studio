import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    required property var device
    required property var appInfo
    contentHeight: column.implicitHeight + Theme.pagePad * 2
    clip: true
    ScrollBar.vertical: ScrollBar {}

    ColumnLayout {
        id: column
        x: Theme.pagePad
        y: Theme.pagePad
        width: page.width - Theme.pagePad * 2
        spacing: 20

        StatusBanner { device: page.device }

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
                Text { Layout.fillWidth: true; text: "Protocol recovered by static analysis of the vendor driver (OemDrv.exe, SINO WEALTH G5KB family). Lighting, sleep and reset writes were verified on hardware earlier; key matrix, macro and per-key colour uploads follow the vendor encoding but are unverified until tested over USB. Every write is read back."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
            }
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
