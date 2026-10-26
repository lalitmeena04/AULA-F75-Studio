import QtQuick

// Scalable 75% keyboard drawn from the vendor key table. `colors` maps LED
// index -> colour string; `labels` maps LED index -> override caption.
Item {
    id: view
    required property var keys
    property var colors: ({})
    property var labels: ({})
    property var selected: ({})          // led -> true
    property int hoveredLed: -1
    property bool showColors: true
    property bool selectable: true
    property color baseKey: Theme.surfaceRaised
    signal keyClicked(int led, int modifiers)
    signal keyDragged(int led)

    readonly property real unit: Math.min(width / 660, height / 290)
    implicitHeight: width * 290 / 660

    Rectangle {
        id: body
        width: 660 * view.unit
        height: 290 * view.unit
        anchors.centerIn: parent
        radius: 18 * view.unit
        color: "#0d1014"
        border.width: 1
        border.color: Theme.border

        Repeater {
            model: view.keys
            delegate: Rectangle {
                id: keyCap
                required property var modelData
                readonly property int led: modelData.led
                readonly property bool isSel: view.selected[led] === true
                readonly property string keyColor: view.showColors && view.colors[led] !== undefined ? view.colors[led] : ""
                x: modelData.x * view.unit
                y: modelData.y * view.unit
                width: modelData.w * view.unit
                height: modelData.h * view.unit
                radius: (modelData.isKnob ? width / 2 : 5) * view.unit
                color: keyColor !== "" ? Qt.alpha(keyColor, 0.28) : view.baseKey
                border.width: isSel ? 2 : 1
                border.color: isSel ? Theme.accent : (view.hoveredLed === led ? Theme.borderStrong : Qt.alpha(Theme.border, 0.9))

                Rectangle {
                    // Underglow bar for the key colour.
                    visible: keyCap.keyColor !== ""
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    anchors.margins: 2 * view.unit
                    height: Math.max(2, 3 * view.unit)
                    radius: height / 2
                    color: keyCap.keyColor === "" ? "transparent" : keyCap.keyColor
                }
                Text {
                    anchors.centerIn: parent
                    width: parent.width - 4
                    text: view.labels[keyCap.led] !== undefined ? view.labels[keyCap.led] : keyCap.modelData.label
                    color: keyCap.isSel ? Theme.text : (keyCap.keyColor !== "" ? Qt.lighter(keyCap.keyColor, 1.5) : Theme.textMuted)
                    font.pixelSize: Math.max(7, (text.length > 4 ? 7 : 10) * view.unit)
                    font.family: Theme.fontFamily
                    horizontalAlignment: Text.AlignHCenter
                    elide: Text.ElideRight
                }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    enabled: view.selectable
                    onEntered: view.hoveredLed = keyCap.led
                    onExited: if (view.hoveredLed === keyCap.led) view.hoveredLed = -1
                    onClicked: (mouse) => view.keyClicked(keyCap.led, mouse.modifiers)
                    onPositionChanged: (mouse) => { if (pressed) view.keyDragged(keyCap.led) }
                }
            }
        }
    }
}
