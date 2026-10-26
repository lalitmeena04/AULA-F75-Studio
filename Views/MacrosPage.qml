import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    required property var work
    contentHeight: column.implicitHeight + Theme.pagePad * 2
    clip: true
    ScrollBar.vertical: ScrollBar {}

    property int selected: -1
    property bool recording: false
    property double lastStamp: 0
    readonly property var macro: selected >= 0 && selected < work.macros.length ? work.macros[selected] : null

    function updateMacro(mutator) {
        const list = JSON.parse(JSON.stringify(work.macros))
        mutator(list[selected])
        work.setMacros(list)
    }
    function addEvent(kind, code, release) {
        const now = Date.now()
        const delay = recording && lastStamp > 0 ? Math.min(0xfffff, now - lastStamp) : 50
        lastStamp = now
        updateMacro((m) => m.events.push({ kind: kind, code: code, release: release, delayMs: delay }))
    }
    function describe(e) {
        const kinds = ["Key", "Modifier", "Mouse", "Wheel", "Reserved", "Move"]
        let what = kinds[e.kind] + " 0x" + e.code.toString(16).padStart(2, "0")
        if (e.kind === 0 || e.kind === 1) what = device.describeAction(device.encodeKeyboard(0, e.code, 0))
        if (e.kind === 2) what = e.code === 1 ? "Left button" : e.code === 2 ? "Right button" : e.code === 4 ? "Middle button" : "Mouse 0x" + e.code.toString(16)
        return what + (e.kind <= 2 ? (e.release ? " ↑" : " ↓") : "")
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
            spacing: 20

            Card {
                Layout.preferredWidth: 320
                Layout.alignment: Qt.AlignTop
                title: "Macros in profile"
                iconName: "macro"
                Repeater {
                    model: work.macros
                    delegate: Rectangle {
                        id: row
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        height: 44
                        radius: Theme.radiusSmall
                        color: page.selected === index ? Qt.alpha(Theme.accent, 0.14) : h.containsMouse ? Theme.surfaceRaised : Theme.bgRaised
                        border.width: 1; border.color: page.selected === index ? Theme.accent : Theme.border
                        RowLayout {
                            anchors.fill: parent; anchors.margins: 10
                            Text { text: (row.index + 1) + "."; color: Theme.textFaint; font.pixelSize: 12; font.family: Theme.monoFamily }
                            Text { Layout.fillWidth: true; text: row.modelData.name; color: Theme.text; font.pixelSize: 13; font.family: Theme.fontFamily; elide: Text.ElideRight }
                            Text { text: row.modelData.events.length + " ev"; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        }
                        MouseArea { id: h; anchors.fill: parent; hoverEnabled: true; onClicked: { page.selected = row.index; page.recording = false } }
                    }
                }
                Text { visible: work.macros.length === 0; text: "No macros yet."; color: Theme.textFaint; font.pixelSize: 12; font.family: Theme.fontFamily }
                RowLayout {
                    AccentButton {
                        text: "New macro"; iconName: "add"; primary: false
                        enabled: work.macros.length < 32
                        onClicked: { const list = JSON.parse(JSON.stringify(work.macros)); list.push({ name: "Macro " + (list.length + 1), events: [] }); work.setMacros(list); page.selected = list.length - 1 }
                    }
                    AccentButton { text: "Read onboard"; iconName: "download"; primary: false; enabled: device.verified && !device.busy; onClicked: device.readMacros() }
                }
                AccentButton { text: "Upload all to keyboard"; iconName: "upload"; enabled: device.verified && !device.busy && work.macros.length > 0; onClicked: device.applyMacros(work.macros) }
                Text { visible: device.macros.length > 0; text: "Keyboard reports " + device.macros.length + " onboard macro(s)."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
            }

            Card {
                visible: page.macro !== null
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignTop
                title: "Editor"
                iconName: "edit"
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    TextField {
                        Layout.fillWidth: true
                        text: page.macro ? page.macro.name : ""
                        maximumLength: 30
                        color: Theme.text; font.pixelSize: 14; font.family: Theme.fontFamily
                        background: Rectangle { radius: Theme.radiusSmall; color: Theme.bgRaised; border.width: 1; border.color: parent.activeFocus ? Theme.accent : Theme.border }
                        onEditingFinished: if (page.macro && text !== page.macro.name) page.updateMacro((m) => m.name = text)
                    }
                    AccentButton { text: "Duplicate"; primary: false; iconName: "copy"; enabled: work.macros.length < 32; onClicked: { const list = JSON.parse(JSON.stringify(work.macros)); list.push(JSON.parse(JSON.stringify(list[page.selected]))); work.setMacros(list) } }
                    AccentButton { text: "Delete"; danger: true; iconName: "delete"; onClicked: { const list = JSON.parse(JSON.stringify(work.macros)); list.splice(page.selected, 1); work.setMacros(list); page.selected = -1 } }
                }

                Rectangle {
                    id: recorder
                    Layout.fillWidth: true
                    height: 70
                    radius: Theme.radiusSmall
                    color: page.recording ? Qt.alpha(Theme.danger, 0.1) : Theme.bgRaised
                    border.width: 1
                    border.color: page.recording ? Theme.danger : (activeFocus ? Theme.accent : Theme.border)
                    focus: true
                    Keys.onPressed: (event) => {
                        if (!page.recording || event.isAutoRepeat) return
                        event.accepted = true
                        const usage = device.usageFromQtKey(event.key)
                        if (usage === 0) return
                        page.addEvent(usage >= 0xE0 ? 1 : 0, usage, false)
                    }
                    Keys.onReleased: (event) => {
                        if (!page.recording || event.isAutoRepeat) return
                        event.accepted = true
                        const usage = device.usageFromQtKey(event.key)
                        if (usage === 0) return
                        page.addEvent(usage >= 0xE0 ? 1 : 0, usage, true)
                    }
                    RowLayout {
                        anchors.fill: parent; anchors.margins: 14
                        spacing: 14
                        IconButton {
                            icon_: page.recording ? "stop" : "record"; danger: !page.recording; accent: page.recording; size: 42; glyphSize: 20
                            tip: page.recording ? "Stop recording" : "Record keystrokes with real timing"
                            onClicked: { page.recording = !page.recording; page.lastStamp = 0; recorder.forceActiveFocus() }
                        }
                        ColumnLayout {
                            spacing: 2
                            Text { text: page.recording ? "Recording… type the sequence, then press Stop" : "Record"; color: Theme.text; font.pixelSize: 13; font.weight: Font.DemiBold; font.family: Theme.fontFamily }
                            Text { text: page.recording ? "Delays between events are captured from your typing." : "Click Record and type. Modifiers, letters, digits, F-keys, arrows and navigation keys are captured."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        }
                        Item { Layout.fillWidth: true }
                        AccentButton { text: "Clear"; primary: false; iconName: "eraser"; onClicked: page.updateMacro((m) => m.events = []) }
                    }
                    MouseArea { anchors.fill: parent; z: -1; onClicked: recorder.forceActiveFocus() }
                }

                Eyebrow { label: "Events (" + (page.macro ? page.macro.events.length : 0) + ")" }
                Repeater {
                    model: page.macro ? page.macro.events : []
                    delegate: RowLayout {
                        id: ev
                        required property var modelData
                        required property int index
                        Layout.fillWidth: true
                        spacing: 10
                        Text { text: (ev.index + 1).toString().padStart(2, "0"); color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.monoFamily; Layout.preferredWidth: 24 }
                        Chip { text: page.describe(ev.modelData); tint: ev.modelData.release ? Theme.textMuted : Theme.accent; outlined: ev.modelData.release }
                        Item { Layout.fillWidth: true }
                        Text { text: "after"; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        SpinBox {
                            from: 0; to: 60000; stepSize: 10
                            value: ev.modelData.delayMs
                            editable: true
                            onValueModified: page.updateMacro((m) => m.events[ev.index].delayMs = value)
                        }
                        Text { text: "ms"; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        IconButton { icon_: "chevron-down"; size: 28; glyphSize: 12; tip: "Move down"; enabled: ev.index < page.macro.events.length - 1; onClicked: page.updateMacro((m) => { const t = m.events[ev.index]; m.events[ev.index] = m.events[ev.index + 1]; m.events[ev.index + 1] = t }) }
                        IconButton { icon_: "delete"; size: 28; glyphSize: 12; danger: true; tip: "Remove"; onClicked: page.updateMacro((m) => m.events.splice(ev.index, 1)) }
                    }
                }
                Text { visible: page.macro && page.macro.events.length === 0; text: "Empty. Record something or insert events below."; color: Theme.textFaint; font.pixelSize: 12; font.family: Theme.fontFamily }
                RowLayout {
                    spacing: 6
                    Eyebrow { label: "Insert" }
                    Repeater {
                        model: [ { t: "Left click", k: 2, c: 1 }, { t: "Right click", k: 2, c: 2 }, { t: "Middle click", k: 2, c: 4 } ]
                        delegate: AccentButton { required property var modelData; text: modelData.t; primary: false; onClicked: { page.recording = false; page.addEvent(modelData.k, modelData.c, false); page.addEvent(modelData.k, modelData.c, true) } }
                    }
                    AccentButton { text: "Delay 500 ms"; primary: false; iconName: "timer"; onClicked: { page.recording = false; page.updateMacro((m) => { if (m.events.length) m.events[m.events.length - 1].delayMs += 500 }) } }
                }
            }
        }
    }
}
