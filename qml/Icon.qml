import QtQuick

// Semantic icon rendered from the active icon font (Fluent, else Font Awesome).
Text {
    id: icon
    required property string name
    property int size: 18
    readonly property var resolved: Icons.resolve(name)

    text: icon.resolved.glyph
    font.family: icon.resolved.family
    font.pixelSize: icon.size
    color: Theme.textMuted
    horizontalAlignment: Text.AlignHCenter
    verticalAlignment: Text.AlignVCenter
    renderType: Text.NativeRendering
}
