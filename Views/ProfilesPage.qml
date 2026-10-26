import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts

Flickable {
    id: page
    required property var work
    contentHeight: column.implicitHeight + Theme.pagePad * 2
    clip: true
    ScrollBar.vertical: ScrollBar {}
    property int exportIndex: -1

    FileDialog {
        id: exportDialog
        fileMode: FileDialog.SaveFile
        nameFilters: ["F75 profile (*.f75.json)"]
        defaultSuffix: "f75.json"
        onAccepted: profileStore.exportTo(page.exportIndex, selectedFile)
    }
    FileDialog {
        id: importDialog
        fileMode: FileDialog.OpenFile
        nameFilters: ["F75 profile (*.f75.json *.json)"]
        onAccepted: profileStore.importFrom(selectedFile)
    }

    ColumnLayout {
        id: column
        x: Theme.pagePad
        y: Theme.pagePad
        width: page.width - Theme.pagePad * 2
        spacing: 20

        StatusBanner { }

        RowLayout {
            Layout.fillWidth: true
            Text { text: "Profiles are stored locally as JSON. Switching the active profile loads it into the editor; Apply sends it to the keyboard."; color: Theme.textMuted; font.pixelSize: 12; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
            AccentButton { text: "Import"; primary: false; iconName: "import"; onClicked: importDialog.open() }
            AccentButton { text: "New profile"; iconName: "add"; onClicked: { const i = profileStore.create(""); profileStore.activeIndex = i } }
        }

        Text { visible: profileStore.lastError !== ""; text: profileStore.lastError; color: Theme.danger; font.pixelSize: 12; font.family: Theme.fontFamily }

        GridLayout {
            Layout.fillWidth: true
            columns: 2
            columnSpacing: 16
            rowSpacing: 16
            Repeater {
                model: profileStore.profiles
                delegate: Card {
                    id: card
                    required property var modelData
                    required property int index
                    readonly property bool active: profileStore.activeIndex === index
                    Layout.fillWidth: true
                    tint: active ? Theme.accent : "transparent"
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        Rectangle {
                            width: 44; height: 44; radius: 12
                            color: Qt.alpha(Theme.accent, card.active ? 0.2 : 0.08)
                            Text { anchors.centerIn: parent; text: (card.index + 1).toString(); color: Theme.accent; font.pixelSize: 16; font.weight: Font.Bold; font.family: Theme.fontFamily }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 2
                            TextField {
                                Layout.fillWidth: true
                                text: card.modelData.name
                                maximumLength: 48
                                color: Theme.text; font.pixelSize: 15; font.weight: Font.DemiBold; font.family: Theme.fontFamily
                                background: Rectangle { color: parent.activeFocus ? Theme.bgRaised : "transparent"; radius: 6; border.width: parent.activeFocus ? 1 : 0; border.color: Theme.accent }
                                onEditingFinished: if (text !== card.modelData.name) profileStore.rename(card.index, text)
                            }
                            Flow {
                                Layout.fillWidth: true
                                spacing: 6
                                Chip { text: device.effects[card.modelData.lighting.effectIndex].name; iconName: "lighting"; tint: Theme.textMuted; outlined: true }
                                Chip { text: Object.keys(card.modelData.keymap).reduce((n, l) => n + Object.keys(card.modelData.keymap[l]).length, 0) + " remaps"; iconName: "keys"; tint: Theme.textMuted; outlined: true }
                                Chip { text: card.modelData.macros.length + " macros"; iconName: "macro"; tint: Theme.textMuted; outlined: true }
                                Chip { text: card.modelData.sleepSeconds === 0 ? "never sleeps" : "sleep " + card.modelData.sleepSeconds + " s"; iconName: "sleep"; tint: Theme.textMuted; outlined: true }
                                Chip { visible: card.active; text: "Active"; iconName: "ok"; tint: Theme.ok }
                            }
                        }
                    }
                    RowLayout {
                        spacing: 6
                        AccentButton { visible: !card.active; text: "Make active"; primary: false; iconName: "star"; onClicked: profileStore.activeIndex = card.index }
                        AccentButton { text: "Apply to keyboard"; iconName: "upload"; enabled: device.verified && !device.busy; onClicked: device.applyProfile(card.active ? work.snapshot() : card.modelData) }
                        Item { Layout.fillWidth: true }
                        IconButton { icon_: "copy"; tip: "Duplicate"; onClicked: profileStore.duplicate(card.index) }
                        IconButton { icon_: "export"; tip: "Export…"; onClicked: { page.exportIndex = card.index; exportDialog.open() } }
                        IconButton { icon_: "delete"; tip: "Delete"; danger: true; enabled: profileStore.profiles.length > 1; onClicked: profileStore.remove(card.index) }
                    }
                }
            }
        }
    }
}
