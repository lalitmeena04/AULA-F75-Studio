import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Flickable {
    id: page
    required property var device
    required property var work
    required property var hostEffects
    contentHeight: column.implicitHeight + Theme.pagePad * 2
    clip: true
    ScrollBar.vertical: ScrollBar {}

    readonly property var effect: device.effects[work.effectIndex]
    readonly property bool custom: work.effectIndex === 18
    property var selectedKeys: ({})
    property color brushColor: "#2ec4ff"
    property string tool: "brush"   // brush, fill, erase
    property var livePreview: ({})

    function selectedCount() { return Object.keys(selectedKeys).length }
    function paint(led) {
        if (tool === "erase") work.setKeyColor(led, "")
        else work.setKeyColor(led, brushColor.toString())
    }
    function paintSelection() {
        const next = Object.assign({}, work.perKey)
        for (const led in selectedKeys) { if (tool === "erase") delete next[led]; else next[led] = brushColor.toString() }
        work.setKeyColors(next)
    }
    function fillAll() {
        const next = {}
        for (const k of device.keys) next[k.led] = brushColor.toString()
        work.setKeyColors(next)
    }

    Connections {
        target: hostEffects
        function onFrameReady(frame) {
            const map = {}
            for (let i = 0; i < frame.length; ++i) map[i] = frame[i].toString()
            page.livePreview = map
        }
    }

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

            // Effect picker
            Card {
                Layout.preferredWidth: 320
                Layout.alignment: Qt.AlignTop
                title: "Effect"
                iconName: "lighting"
                GridLayout {
                    Layout.fillWidth: true
                    columns: 2
                    columnSpacing: 8
                    rowSpacing: 8
                    Repeater {
                        model: device.effects
                        delegate: Rectangle {
                            id: tile
                            required property var modelData
                            readonly property bool active: work.effectIndex === modelData.index
                            Layout.fillWidth: true
                            height: 40
                            radius: Theme.radiusSmall
                            color: active ? Qt.alpha(Theme.accent, 0.16) : hover.containsMouse ? Theme.surfaceRaised : Theme.bgRaised
                            border.width: 1
                            border.color: active ? Theme.accent : Theme.border
                            Text { anchors.centerIn: parent; width: parent.width - 12; text: tile.modelData.name; color: tile.active ? Theme.text : Theme.textMuted; font.pixelSize: 11; font.family: Theme.fontFamily; elide: Text.ElideRight; horizontalAlignment: Text.AlignHCenter }
                            MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true; onClicked: work.setLighting(tile.modelData.index, work.brightness, work.speed, work.color) }
                        }
                    }
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 20

                Card {
                    Layout.fillWidth: true
                    title: page.custom ? "Per-key canvas" : "Preview"
                    iconName: page.custom ? "brush" : "keyboard"
                    RowLayout {
                        visible: page.custom
                        Layout.fillWidth: true
                        spacing: 8
                        IconButton { icon_: "brush"; tip: "Brush: click or drag keys"; accent: page.tool === "brush"; onClicked: page.tool = "brush" }
                        IconButton { icon_: "eraser"; tip: "Erase key colour"; accent: page.tool === "erase"; onClicked: page.tool = "erase" }
                        IconButton { icon_: "fill"; tip: "Fill every key with the brush colour"; onClicked: page.fillAll() }
                        IconButton { icon_: "delete"; tip: "Clear all key colours"; danger: true; onClicked: work.setKeyColors({}) }
                        Item { Layout.fillWidth: true }
                        Text { text: page.selectedCount() > 0 ? page.selectedCount() + " selected · Ctrl+click to add" : "Ctrl+click keys to multi-select"; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        AccentButton { visible: page.selectedCount() > 0; text: "Paint selection"; primary: false; iconName: "brush"; onClicked: page.paintSelection() }
                        AccentButton { visible: page.selectedCount() > 0; text: "Clear selection"; primary: false; onClicked: page.selectedKeys = ({}) }
                    }
                    KeyboardView {
                        Layout.fillWidth: true
                        Layout.preferredHeight: width * 290 / 660
                        keys: device.keys
                        colors: hostEffects.mode !== "off" ? page.livePreview : page.custom ? work.perKey : ({})
                        selected: page.selectedKeys
                        selectable: page.custom && hostEffects.mode === "off"
                        onKeyClicked: (led, mods) => {
                            if (mods & Qt.ControlModifier) {
                                const next = Object.assign({}, page.selectedKeys)
                                if (next[led]) delete next[led]; else next[led] = true
                                page.selectedKeys = next
                            } else {
                                page.selectedKeys = ({})
                                page.paint(led)
                            }
                        }
                        onKeyDragged: (led) => page.paint(led)
                    }
                    Text {
                        visible: !page.custom
                        text: page.effect ? page.effect.name + " runs on the keyboard's own animation engine; colour and speed are set below." : ""
                        color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    Card {
                        Layout.fillWidth: true
                        Layout.alignment: Qt.AlignTop
                        title: "Parameters"
                        iconName: "settings"
                        LabeledSlider {
                            Layout.fillWidth: true
                            label: "Brightness"; iconName: "brightness"; from: 0; to: 9
                            enabled: page.effect ? page.effect.hasBrightness : false
                            value: work.brightness
                            onCommitted: (v) => work.setLighting(work.effectIndex, v, work.speed, work.color)
                        }
                        LabeledSlider {
                            Layout.fillWidth: true
                            label: "Speed"; iconName: "speed"; from: 0; to: 4
                            enabled: page.effect ? page.effect.hasSpeed : false
                            value: work.speed
                            onCommitted: (v) => work.setLighting(work.effectIndex, work.brightness, v, work.color)
                        }
                        Eyebrow { label: "Colour"; visible: page.effect ? page.effect.hasColor : false }
                        Flow {
                            visible: page.effect ? page.effect.hasColor : false
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: device.colors
                                delegate: Rectangle {
                                    id: sw
                                    required property var modelData
                                    required property int index
                                    width: 34; height: 34; radius: 10
                                    color: index === 7 ? "transparent" : modelData.color
                                    border.width: work.color === index ? 3 : 1
                                    border.color: work.color === index ? Theme.text : Theme.borderStrong
                                    gradient: index === 7 ? rainbow : null
                                    Gradient { id: rainbow; GradientStop { position: 0; color: "#ff3b30" } GradientStop { position: 0.5; color: "#34c759" } GradientStop { position: 1; color: "#2ec4ff" } }
                                    ToolTip.visible: hover.containsMouse; ToolTip.text: sw.modelData.name; ToolTip.delay: 400
                                    MouseArea { id: hover; anchors.fill: parent; hoverEnabled: true; onClicked: work.setLighting(work.effectIndex, work.brightness, work.speed, sw.index) }
                                }
                            }
                        }
                        RowLayout {
                            Layout.topMargin: 6
                            AccentButton {
                                text: page.custom ? "Apply per-key colours" : "Apply effect"
                                iconName: "upload"
                                enabled: device.verified && !device.busy
                                onClicked: page.custom ? device.applyPerKey(work.perKey) : device.applyEffect(work.effectIndex, work.brightness, work.speed, work.color)
                            }
                            Text { visible: !device.verified; text: "Connect over USB to apply."; color: Theme.textFaint; font.pixelSize: 11; font.family: Theme.fontFamily }
                        }
                    }

                    Card {
                        visible: page.custom
                        Layout.preferredWidth: 300
                        Layout.alignment: Qt.AlignTop
                        title: "Brush"
                        iconName: "color"
                        ColorField { Layout.fillWidth: true; color: page.brushColor; onPicked: (c) => page.brushColor = c }
                    }
                }

                // Host-streamed lighting
                Card {
                    Layout.fillWidth: true
                    title: "Live lighting from this PC"
                    iconName: "music"
                    tint: hostEffects.mode !== "off" ? Theme.accent : "transparent"
                    Text {
                        Layout.fillWidth: true
                        text: "Streams a transient per-key frame to the keyboard (the vendor's real-time command, nothing is written to flash) while this app runs. Audio uses WASAPI loopback of the default output; heatmap uses CPU load, GPU 3D utilisation and NVIDIA GPU temperature when available."
                        color: Theme.textMuted; font.pixelSize: 11; font.family: Theme.fontFamily; wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        spacing: 8
                        Repeater {
                            model: [ { id: "off", label: "Off", icon: "stop" }, { id: "audio", label: "Audio reactive", icon: "audio" }, { id: "heatmap", label: "CPU / GPU heatmap", icon: "heat" } ]
                            delegate: AccentButton {
                                required property var modelData
                                text: modelData.label; iconName: modelData.icon
                                primary: hostEffects.mode === modelData.id
                                onClicked: {
                                    hostEffects.mode = modelData.id
                                    if (modelData.id === "off") device.stopStream()
                                    else if (device.verified && !device.streaming) device.startStream(hostEffects)
                                }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Chip { visible: device.streaming; text: "Streaming to keyboard"; iconName: "upload"; tint: Theme.ok }
                        Chip { visible: hostEffects.mode !== "off" && !device.streaming; text: device.verified ? "Preview only" : "Preview only · connect USB to stream"; iconName: "info"; tint: Theme.warn }
                    }
                    RowLayout {
                        visible: hostEffects.mode !== "off"
                        Layout.fillWidth: true
                        spacing: 20
                        LabeledSlider { Layout.fillWidth: true; label: "Gain"; from: 1; to: 40; value: hostEffects.gain * 10; valueText: (hostEffects.gain).toFixed(1) + "×"; live: true; onCommitted: (v) => hostEffects.gain = v / 10 }
                        LabeledSlider { Layout.fillWidth: true; label: "Frames / s"; from: 2; to: 20; value: hostEffects.fps; onCommitted: (v) => hostEffects.fps = v }
                        ColumnLayout {
                            spacing: 4
                            CheckBox { text: "Rainbow"; checked: hostEffects.rainbow; onToggled: hostEffects.rainbow = checked; contentItem: Text { text: parent.text; color: Theme.text; font.pixelSize: 12; font.family: Theme.fontFamily; leftPadding: parent.indicator.width + 6; verticalAlignment: Text.AlignVCenter } }
                            Text { visible: hostEffects.mode === "heatmap"; text: "CPU " + hostEffects.telemetry.cpu.toFixed(0) + "%  ·  GPU " + hostEffects.telemetry.gpu.toFixed(0) + "%" + (hostEffects.telemetry.gpuTemp > 0 ? "  ·  " + hostEffects.telemetry.gpuTemp.toFixed(0) + " °C" : "  ·  temp n/a"); color: Theme.textMuted; font.pixelSize: 11; font.family: Theme.monoFamily }
                            Text { visible: hostEffects.status !== ""; text: hostEffects.status; color: Theme.warn; font.pixelSize: 11; font.family: Theme.fontFamily }
                        }
                    }
                }
            }
        }
    }
}
