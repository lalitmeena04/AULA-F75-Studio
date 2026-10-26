import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    required property var work
    contentHeight: column.implicitHeight + Theme.pagePad * 2
    clip: true
    ScrollBar.vertical: ScrollBar {}

    property int activeLayer: 0
    property int selectedLed: -1
    property int capturedUsage: 0
    property int capturedMods: 0
    property string assignTab: "key"   // key, media, mouse, macro, device

    readonly property var layerMap: work.keymap[activeLayer] !== undefined ? work.keymap[activeLayer] : ({})
    readonly property var deviceLayer: device.keymap[activeLayer] !== undefined ? device.keymap[activeLayer] : ({})

    function codeFor(led) {
        if (layerMap[led] !== undefined) return layerMap[led]
        if (deviceLayer[led] !== undefined) return deviceLayer[led]
        return device.defaultAction(activeLayer, led)
    }
    function labelsForLayer() {
        const out = {}
        if (activeLayer === 1) {
            // Fn layer: caption every key with its current function so the shortcut table is visible.
            for (const k of device.keys) {
                const code = codeFor(k.led)
                out[k.led] = code === 0 ? "—" : device.describeAction(code)
            }
        }
        for (const led in layerMap) out[led] = device.describeAction(layerMap[led])
        return out
    }
    function colorsForLayer() {
        const out = {}
        if (activeLayer === 1)
            for (const k of device.keys) if (deviceLayer[k.led] !== undefined || device.defaultAction(1, k.led) !== 0) out[k.led] = Theme.textFaint.toString()
        for (const led in layerMap) out[led] = Theme.accent.toString()
        return out
    }
    function currentCode() { return selectedLed < 0 ? 0 : codeFor(selectedLed) }
    function assign(code) { if (selectedLed >= 0) work.setKey(activeLayer, selectedLed, code) }
    function selectedLabel() {
        for (const k of device.keys) {
            if (k.led === selectedLed) return k.label === "" ? "Space" : k.label
        }
        return ""
    }

    ColumnLayout {
        id: column
        x: Theme.pagePad
        y: Theme.pagePad
        width: page.width - Theme.pagePad * 2
        spacing: 20

        StatusBanner { }

        Card {
            Layout.fillWidth: true
            RowLayout {
                Layout.fillWidth: true
                spacing: 10
                Eyebrow { label: "Layer" }
                Repeater {
                    model: [ { i: 0, t: "Base" }, { i: 1, t: "Fn" } ]
                    delegate: AccentButton {
                        required property var modelData
                        text: modelData.t; primary: page.activeLayer === modelData.i; iconName: "layers"
                        onClicked: { page.activeLayer = modelData.i; page.selectedLed = -1 }
                    }
                }
                Item { Layout.fillWidth: true }
                Text { text: Object.keys(page.layerMap).length + " remapped on this layer"; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                AccentButton { text: "Read from keyboard"; primary: false; iconName: "download"; enabled: device.verified && !device.busy; onClicked: device.readKeymap(page.activeLayer) }
                AccentButton { text: "Reset layer"; primary: false; iconName: "revert"; enabled: Object.keys(page.layerMap).length > 0; onClicked: work.clearLayer(page.activeLayer) }
                AccentButton { text: "Apply layer"; iconName: "upload"; enabled: device.verified && !device.busy; onClicked: device.applyKeymap(page.activeLayer, page.layerMap) }
            }
            KeyboardView {
                Layout.fillWidth: true
                Layout.preferredHeight: width * 290 / 660
                keys: device.keys
                labels: page.labelsForLayer()
                colors: page.colorsForLayer()
                selected: page.selectedLed >= 0 ? ({ [page.selectedLed]: true }) : ({})
                onKeyClicked: (led) => { page.selectedLed = led; page.capturedUsage = 0; page.capturedMods = 0; capture.forceActiveFocus() }
            }
            Text {
                Layout.fillWidth: true
                text: page.activeLayer === 1 ? "Fn layer: keys without an override keep the keyboard's factory Fn function (media keys, lighting controls, Insert/PrtSc/ScrLk/Pause on Del/U/I/O…)."
                                       : "Base layer: pick a key, then choose what it should do. The Fn key itself cannot be moved. Applying writes the whole layer; unlisted keys get their factory function."
                color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap
            }
        }

        RowLayout {
            visible: page.selectedLed >= 0
            Layout.fillWidth: true
            spacing: 20

            Card {
                Layout.preferredWidth: 300
                Layout.alignment: Qt.AlignTop
                title: "Selected key"
                iconName: "keyboard"
                Text { text: page.selectedLabel(); color: Theme.text; font.pixelSize: 28; font.weight: Font.Bold; font.family: Theme.fontFamily }
                Eyebrow { label: "Currently" }
                Text { text: device.describeAction(page.currentCode()); color: Theme.accent; font.pixelSize: 14; font.weight: Font.DemiBold; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                Text { text: "LED index " + page.selectedLed + " · word 0x" + page.currentCode().toString(16).padStart(8, "0"); color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.monoFamily }
                RowLayout {
                    AccentButton { text: "Factory"; primary: false; iconName: "revert"; onClicked: work.setKey(page.activeLayer, page.selectedLed, undefined) }
                    AccentButton { text: "Disable"; primary: false; iconName: "close"; onClicked: page.assign(0) }
                }
            }

            Card {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                title: "Assign"
                iconName: "keys"
                RowLayout {
                    spacing: 6
                    Repeater {
                        model: [ { id: "key", t: "Keyboard" }, { id: "media", t: "Media & apps" }, { id: "mouse", t: "Mouse" }, { id: "macro", t: "Macro" }, { id: "device", t: "Device" } ]
                        delegate: AccentButton { required property var modelData; text: modelData.t; primary: page.assignTab === modelData.id; onClicked: page.assignTab = modelData.id }
                    }
                }

                // Keyboard: capture a shortcut or pick from the list
                ColumnLayout {
                    visible: page.assignTab === "key"
                    Layout.fillWidth: true
                    spacing: 10
                    Rectangle {
                        id: capture
                        Layout.fillWidth: true
                        height: 64
                        radius: Theme.radiusSmall
                        color: activeFocus ? Qt.alpha(Theme.accent, 0.1) : Theme.bgRaised
                        border.width: 1
                        border.color: activeFocus ? Theme.accent : Theme.border
                        focus: true
                        Keys.onPressed: (event) => {
                            event.accepted = true
                            const usage = device.usageFromQtKey(event.key)
                            if (usage >= 0xE0 && usage <= 0xE7) {
                                page.capturedMods |= (1 << (usage - 0xE0))
                                page.capturedUsage = 0
                                page.assign(device.encodeKeyboard(page.capturedMods, 0, 0))   // modifier-only until a key follows
                                return
                            }
                            if (usage === 0) return
                            page.capturedUsage = usage
                            let mods = 0
                            if (event.modifiers & Qt.ControlModifier) mods |= 1
                            if (event.modifiers & Qt.ShiftModifier) mods |= 2
                            if (event.modifiers & Qt.AltModifier) mods |= 4
                            if (event.modifiers & Qt.MetaModifier) mods |= 8
                            page.capturedMods = mods
                            page.assign(device.encodeKeyboard(mods, usage, 0))
                        }
                        Column {
                            anchors.centerIn: parent
                            spacing: 2
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: capture.activeFocus ? "Press a key or shortcut…" : "Click here, then press a key or shortcut"; color: capture.activeFocus ? Theme.text : Theme.textMuted; font.pixelSize: 13; font.family: Theme.fontFamily }
                            Text { anchors.horizontalCenter: parent.horizontalCenter; text: page.capturedUsage ? "Captured: " + device.describeAction(device.encodeKeyboard(page.capturedMods, page.capturedUsage, 0)) : "Modifiers held while pressing are included."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        }
                        MouseArea { anchors.fill: parent; onClicked: capture.forceActiveFocus() }
                    }
                    Eyebrow { label: "Or pick" }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 4
                        Repeater {
                            model: device.hidUsages
                            delegate: Rectangle {
                                required property var modelData
                                width: t.implicitWidth + 16; height: 26; radius: 6
                                color: h.containsMouse ? Theme.surfaceHover : Theme.bgRaised; border.width: 1; border.color: Theme.border
                                Text { id: t; anchors.centerIn: parent; text: parent.modelData.name; color: Theme.textMuted; font.pixelSize: 11; font.family: Theme.fontFamily }
                                MouseArea { id: h; anchors.fill: parent; hoverEnabled: true; onClicked: page.assign(device.encodeKeyboard(0, parent.modelData.usage, 0)) }
                            }
                        }
                    }
                }

                Flow {
                    visible: page.assignTab === "media"
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: device.consumerUsages
                        delegate: AccentButton { required property var modelData; text: modelData.name; primary: false; onClicked: page.assign(device.encodeConsumer(modelData.usage)) }
                    }
                }

                Flow {
                    visible: page.assignTab === "mouse"
                    Layout.fillWidth: true
                    spacing: 6
                    Repeater {
                        model: [ { b: 1, t: "Left click" }, { b: 2, t: "Right click" }, { b: 4, t: "Middle click" }, { b: 8, t: "Back" }, { b: 16, t: "Forward" } ]
                        delegate: AccentButton { required property var modelData; text: modelData.t; primary: false; iconName: "mouse"; onClicked: page.assign(device.encodeMouse(modelData.b)) }
                    }
                }

                ColumnLayout {
                    visible: page.assignTab === "macro"
                    Layout.fillWidth: true
                    spacing: 8
                    Text { visible: work.macros.length === 0; text: "No macros in this profile yet. Create one on the Macros page."; color: Theme.textFaint; font.pixelSize: 12; font.family: Theme.fontFamily }
                    RowLayout {
                        spacing: 8
                        Eyebrow { label: "Trigger" }
                        ComboBox {
                            id: macroMode
                            model: [ "Play N times", "Repeat while held", "Toggle on / off" ]
                            Layout.preferredWidth: 180
                        }
                        SpinBox { id: macroRepeat; from: 1; to: 255; value: 1; visible: macroMode.currentIndex === 0 }
                    }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: work.macros
                            delegate: AccentButton {
                                required property var modelData
                                required property int index
                                text: (index + 1) + ". " + modelData.name; primary: false; iconName: "macro"
                                onClicked: page.assign(device.encodeMacro(index, [1, 2, 4][macroMode.currentIndex], macroRepeat.value))
                            }
                        }
                    }
                    Text { text: "Macros are referenced by their position in the profile; apply macros before applying the layer."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap; Layout.fillWidth: true }
                }

                ColumnLayout {
                    visible: page.assignTab === "device"
                    Layout.fillWidth: true
                    spacing: 8
                    Eyebrow { label: "Keyboard functions (from the factory Fn layer)" }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: device.deviceFunctions
                            delegate: AccentButton { required property var modelData; text: modelData.name; primary: false; iconName: "keyboard"; onClicked: page.assign(modelData.code) }
                        }
                    }
                    Eyebrow { label: "Onboard profile" }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [0, 1, 2, 3]
                            delegate: AccentButton { required property int modelData; text: "Switch to profile " + (modelData + 1); primary: false; iconName: "profiles"; onClicked: page.assign(device.encodeProfile(modelData)) }
                        }
                    }
                    Text { Layout.fillWidth: true; text: "These are the words the keyboard itself uses for Fn+key functions (mode switches, lighting controls, locks). They can be placed on any key of either layer."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap }
                }
            }
        }
    }
}
