pragma Singleton
import QtQuick

QtObject {
    // Near-black canvas, cool cyan accent, tonal surfaces.
    readonly property color bg: "#0a0c0f"
    readonly property color bgRaised: "#101318"
    readonly property color surface: "#151a20"
    readonly property color surfaceRaised: "#1b2129"
    readonly property color surfaceHover: "#222a33"
    readonly property color border: "#252d37"
    readonly property color borderStrong: "#36404c"
    readonly property color text: "#f3f6f9"
    readonly property color textMuted: "#98a4b2"
    readonly property color textFaint: "#66727f"
    readonly property color accent: "#2ec4ff"
    readonly property color accentDeep: "#0f6fae"
    readonly property color ok: "#3ddc97"
    readonly property color warn: "#f7b955"
    readonly property color danger: "#ff5d6c"
    readonly property int radius: 14
    readonly property int radiusSmall: 8
    readonly property int railWidth: 76
    readonly property int topBarHeight: 60
    readonly property int pagePad: 32
    readonly property string fontFamily: "Segoe UI Variable Display"
    readonly property string monoFamily: "Cascadia Mono"
}
