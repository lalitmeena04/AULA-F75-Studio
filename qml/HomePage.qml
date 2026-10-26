import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    required property var device
    required property var work
    required property var profileStore
    signal navigate(int index)
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

        // Hero: keyboard preview with the working profile's colours.
        Card {
            Layout.fillWidth: true
            padding: 26
            RowLayout {
                Layout.fillWidth: true
                spacing: 24
                ColumnLayout {
                    Layout.preferredWidth: 300
                    Layout.maximumWidth: 320
                    spacing: 8
                    Eyebrow { label: "Device" }
                    Text { text: device.current.variant !== undefined ? device.current.variant : "AULA F75"; color: Theme.text; font.pixelSize: 26; font.weight: Font.Bold; font.family: Theme.fontFamily }
                    Text {
                        text: device.link === "usb" ? "USB cable · configuration channel open"
                            : device.link === "receiver" ? "2.4 GHz receiver · typing + status only"
                            : "Not detected"
                        color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Chip { text: device.current.effectName !== undefined ? device.current.effectName : "Lighting unknown"; iconName: "lighting"; tint: Theme.accent }
                        Chip { text: device.current.sleepSeconds > 0 ? "Sleep " + device.current.sleepSeconds + " s" : device.current.sleepSeconds === 0 ? "Never sleeps" : "Sleep ?"; iconName: "sleep"; tint: Theme.textMuted; outlined: true }
                        Chip { text: "VID 258A · PID 010C"; tint: Theme.textFaint; outlined: true }
                    }
                    Item { height: 6 }
                    AccentButton {
                        text: device.verified ? "Re-probe keyboard" : "Connect"
                        iconName: "connect"
                        enabled: !device.busy
                        onClicked: device.probe()
                    }
                    Text {
                        visible: device.link === "receiver"
                        Layout.fillWidth: true
                        text: "The receiver does not expose the 520-byte configuration interface, so lighting, key and macro writes need the USB cable. The status channel below is monitored read-only."
                        color: Theme.warn; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap
                    }
                }
                KeyboardView {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 300
                    keys: device.keys
                    colors: work.effectIndex === 18 ? work.perKey : ({})
                    selectable: false
                }
            }
        }

        GridLayout {
            Layout.fillWidth: true
            columns: 3
            columnSpacing: 16
            rowSpacing: 16
            Repeater {
                model: [
                    { icon: "lighting", title: "Lighting", body: "18 hardware effects, per-key custom mode, audio-reactive and system heatmap streaming.", page: 1 },
                    { icon: "keys", title: "Key remap", body: "Rebind any key on the base or Fn layer to keys, shortcuts, media, mouse buttons or macros.", page: 2 },
                    { icon: "macro", title: "Macros", body: "Record or edit key sequences with delays and upload up to 32 macros onboard.", page: 3 },
                    { icon: "power", title: "Power", body: "Idle sleep timer and receiver status.", page: 4 },
                    { icon: "profiles", title: "Profiles", body: "Local JSON profiles with export and import. Nothing leaves this machine.", page: 5 },
                    { icon: "diagnostics", title: "Diagnostics", body: "Raw config block, HID exchange log and the receiver's 20-byte status reports.", page: 6 },
                ]
                delegate: Card {
                    id: tile
                    required property var modelData
                    Layout.fillWidth: true
                    padding: 20
                    RowLayout {
                        spacing: 12
                        Rectangle { width: 40; height: 40; radius: 12; color: Qt.alpha(Theme.accent, 0.12); Icon { anchors.centerIn: parent; name: tile.modelData.icon; size: 18; color: Theme.accent } }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            Text { text: tile.modelData.title; color: Theme.text; font.pixelSize: 14; font.weight: Font.DemiBold; font.family: Theme.fontFamily }
                            Text { text: tile.modelData.body; color: Theme.textMuted; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                        }
                        Icon { name: "chevron-right"; size: 14 }
                    }
                    TapHandler { onTapped: page.navigate(tile.modelData.page) }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }
            }
        }
    }
}
