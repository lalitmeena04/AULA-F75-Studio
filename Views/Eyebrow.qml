import QtQuick

// Small uppercase section label.
Text {
    property string label: ""
    text: label.toUpperCase()
    color: Theme.textFaint
    font.pixelSize: 11
    font.letterSpacing: 1.4
    font.weight: Font.DemiBold
    font.family: Theme.fontFamily
}
