import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// Left navigation rail with icon buttons.
Rectangle {
    id: rail
    property int currentIndex: 0
    property var items: []
    signal selected(int index)
    width: Theme.railWidth
    color: Theme.bgRaised
    Rectangle { anchors.right: parent.right; width: 1; height: parent.height; color: Theme.border }

    ColumnLayout {
        anchors.fill: parent
        anchors.topMargin: 14
        anchors.bottomMargin: 14
        spacing: 4

        Rectangle {
            Layout.alignment: Qt.AlignHCenter
            Layout.bottomMargin: 14
            width: 40; height: 40; radius: 12
            gradient: Gradient {
                GradientStop { position: 0; color: Theme.accent }
                GradientStop { position: 1; color: Theme.accentDeep }
            }
            Text { anchors.centerIn: parent; text: "F75"; color: "#061018"; font.pixelSize: 13; font.weight: Font.Black; font.family: Theme.fontFamily }
        }

        Repeater {
            model: rail.items
            delegate: Item {
                id: entry
                required property var modelData
                required property int index
                Layout.alignment: Qt.AlignHCenter
                width: 56; height: 56
                readonly property bool active: rail.currentIndex === index
                Rectangle {
                    anchors.fill: parent
                    radius: 14
                    color: entry.active ? Qt.alpha(Theme.accent, 0.14) : mouse.containsMouse ? Theme.surfaceRaised : "transparent"
                    Behavior on color { ColorAnimation { duration: 120 } }
                }
                Rectangle { visible: entry.active; width: 3; height: 24; radius: 2; color: Theme.accent; anchors.left: parent.left; anchors.verticalCenter: parent.verticalCenter }
                Column {
                    anchors.centerIn: parent
                    spacing: 3
                    Icon { name: entry.modelData.icon; size: 20; color: entry.active ? Theme.accent : Theme.textMuted; anchors.horizontalCenter: parent.horizontalCenter }
                    Text { text: entry.modelData.label; color: entry.active ? Theme.text : Theme.textFaint; font.pixelSize: 9; font.family: Theme.fontFamily; anchors.horizontalCenter: parent.horizontalCenter }
                }
                MouseArea { id: mouse; anchors.fill: parent; hoverEnabled: true; onClicked: rail.selected(entry.index) }
            }
        }
        Item { Layout.fillHeight: true }
        // Text { Layout.alignment: Qt.AlignHCenter; text: Icons.activeSet === "fluent" ? "Fluent" : "FA"; color: Theme.textFaint; font.pixelSize: 9; font.family: Theme.fontFamily }
    }
}
