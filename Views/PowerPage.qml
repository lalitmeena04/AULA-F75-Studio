import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    required property var work
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
                title: "Idle sleep"
                iconName: "sleep"
                Text { Layout.fillWidth: true; text: "How long the keyboard waits without input before it sleeps on battery. The keyboard reports " + (device.current.sleepSeconds > 0 ? device.current.sleepSeconds + " s" : device.current.sleepSeconds === 0 ? "never" : "unknown") + "."; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
                Flow {
                    Layout.fillWidth: true
                    spacing: 8
                    Repeater {
                        model: device.sleepPresets
                        delegate: AccentButton {
                            required property var modelData
                            text: modelData.seconds === 0 ? "Never" : modelData.seconds < 60 ? modelData.seconds + " s" : (modelData.seconds / 60) + " min"
                            primary: work.sleepSeconds === modelData.seconds
                            onClicked: work.setSleep(modelData.seconds)
                        }
                    }
                }
                RowLayout {
                    AccentButton { text: "Apply sleep timer"; iconName: "upload"; enabled: device.verified && !device.busy; onClicked: device.applySleep(work.sleepSeconds) }
                    Text { text: "\"Never\" is inferred (hardware value 0) and has not been confirmed on a keyboard."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }

            Card {
                Layout.preferredWidth: 420
                Layout.alignment: Qt.AlignTop
                title: "Receiver status channel"
                iconName: "wireless"
                tint: device.receiverStatus.present ? Theme.ok : "transparent"
                Text { Layout.fillWidth: true; text: device.receiverStatus.message !== undefined ? device.receiverStatus.message : "Not monitored."; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
                RowLayout {
                    visible: device.receiverStatus.count !== undefined
                    Chip { text: device.receiverStatus.count + " reports"; iconName: "download"; tint: Theme.accent }
                    Chip { visible: device.receiverStatus.lastAt !== undefined; text: "last " + device.receiverStatus.lastAt; tint: Theme.textMuted; outlined: true }
                }
                Text { visible: device.receiverStatus.lastHex !== undefined; text: device.receiverStatus.lastHex !== undefined ? device.receiverStatus.lastHex : ""; color: Theme.text; font.pixelSize: 12; font.family: Theme.monoFamily; wrapMode: Text.Wrap; Layout.fillWidth: true }
                Text { Layout.fillWidth: true; text: "Battery level: not decoded yet. Raw status reports are shown so the layout can be learned by watching values change while charging."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 20

            Card {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                title: "Game mode"
                iconName: "game"
                tint: gameMode.enabled ? Theme.accent : "transparent"
                Text { Layout.fillWidth: true; text: "Keeps shortcuts that steal focus from reaching games while this app runs. Nothing is written to the keyboard; the lock lifts when you turn it off or close the app. The keyboard's own Fn + Win lock works independently."; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
                Flow {
                    Layout.fillWidth: true
                    spacing: 16
                    CheckBox { text: "Windows keys"; checked: gameMode.blockWin; onToggled: gameMode.blockWin = checked; contentItem: Text { text: parent.text; color: Theme.text; font.pixelSize: 12; font.family: Theme.fontFamily; leftPadding: parent.indicator.width + 6; verticalAlignment: Text.AlignVCenter } }
                    CheckBox { text: "Alt + Tab"; checked: gameMode.blockAltTab; onToggled: gameMode.blockAltTab = checked; contentItem: Text { text: parent.text; color: Theme.text; font.pixelSize: 12; font.family: Theme.fontFamily; leftPadding: parent.indicator.width + 6; verticalAlignment: Text.AlignVCenter } }
                    CheckBox { text: "Alt + F4"; checked: gameMode.blockAltF4; onToggled: gameMode.blockAltF4 = checked; contentItem: Text { text: parent.text; color: Theme.text; font.pixelSize: 12; font.family: Theme.fontFamily; leftPadding: parent.indicator.width + 6; verticalAlignment: Text.AlignVCenter } }
                }
                RowLayout {
                    AccentButton {
                        text: gameMode.enabled ? "Turn off game mode" : "Turn on game mode"
                        iconName: gameMode.enabled ? "unlock" : "lock"
                        primary: !gameMode.enabled
                        enabled: gameMode.enabled || gameMode.blockWin || gameMode.blockAltTab || gameMode.blockAltF4
                        onClicked: gameMode.enabled = !gameMode.enabled
                    }
                    Chip { visible: gameMode.enabled; text: gameMode.blockedCount + " blocked"; iconName: "lock"; tint: Theme.accent }
                    Text { visible: !gameMode.enabled && gameMode.status !== "Off"; text: gameMode.status; color: Theme.warn; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }
            }

            Card {
                Layout.preferredWidth: 420
                Layout.alignment: Qt.AlignTop
                title: "Factory reset"
                iconName: "reset"
                tint: Theme.danger
                Text { Layout.fillWidth: true; text: "Sends the keyboard's factory-reset command (0x11). Lighting, key map and macros stored on the keyboard return to defaults. Your local profiles are untouched."; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
                RowLayout {
                    CheckBox { id: confirmReset; text: "I understand"; contentItem: Text { text: parent.text; color: Theme.text; font.pixelSize: 12; font.family: Theme.fontFamily; leftPadding: parent.indicator.width + 6; verticalAlignment: Text.AlignVCenter } }
                    AccentButton { text: "Reset keyboard"; danger: true; iconName: "reset"; enabled: confirmReset.checked && device.verified && !device.busy; onClicked: { device.factoryReset(); confirmReset.checked = false } }
                }
            }
        }
    }
}
