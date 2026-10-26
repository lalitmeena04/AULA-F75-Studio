import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    required property var device
    required property var hostEffects
    required property var profileStore
    required property var appInfo
    property int currentPage: 0

    width: 1320
    height: 860
    minimumWidth: 1080
    minimumHeight: 700
    visible: true
    title: "AULA F75 Studio"
    color: Theme.bg
    font.family: Theme.fontFamily

    readonly property var pages: [
        { label: "Home", icon: "home" },
        { label: "Lighting", icon: "lighting" },
        { label: "Keys", icon: "keys" },
        { label: "Macros", icon: "macro" },
        { label: "Power", icon: "power" },
        { label: "Profiles", icon: "profiles" },
        { label: "Debug", icon: "diagnostics" },
    ]

    // Working copy of the active profile. Pages edit this; Save persists it,
    // Apply sends it (or a part of it) to the keyboard.
    QtObject {
        id: work
        property int sourceIndex: -1
        property string name: ""
        property int effectIndex: 9
        property int brightness: 7
        property int speed: 2
        property int color: 7
        property var perKey: ({})
        property var keymap: ({})
        property var macros: []
        property int sleepSeconds: 300
        property bool dirty: false

        function load(index) {
            const p = profileStore.profile(index)
            sourceIndex = index
            name = p.name
            effectIndex = p.lighting.effectIndex
            brightness = p.lighting.brightness
            speed = p.lighting.speed
            color = p.lighting.color
            perKey = Object.assign({}, p.perKey)
            keymap = JSON.parse(JSON.stringify(p.keymap))
            macros = JSON.parse(JSON.stringify(p.macros))
            sleepSeconds = p.sleepSeconds
            dirty = false
        }
        function snapshot() {
            return { name: name, lighting: { effectIndex: effectIndex, brightness: brightness, speed: speed, color: color },
                     perKey: perKey, keymap: keymap, macros: macros, sleepSeconds: sleepSeconds }
        }
        function save() {
            if (sourceIndex < 0) return
            profileStore.update(sourceIndex, snapshot())
            dirty = false
        }
        function setLighting(e, b, s, c) { effectIndex = e; brightness = b; speed = s; color = c; dirty = true }
        function setKeyColor(led, value) {
            const next = Object.assign({}, perKey)
            if (value === "" || value === undefined) delete next[led]; else next[led] = value
            perKey = next; dirty = true
        }
        function setKeyColors(map) { perKey = map; dirty = true }
        function setKey(layer, led, code) {
            const next = JSON.parse(JSON.stringify(keymap))
            if (!next[layer]) next[layer] = {}
            if (code === undefined || code === null) delete next[layer][led]; else next[layer][led] = code
            keymap = next; dirty = true
        }
        function clearLayer(layer) { const next = JSON.parse(JSON.stringify(keymap)); delete next[layer]; keymap = next; dirty = true }
        function setMacros(list) { macros = JSON.parse(JSON.stringify(list)); dirty = true }
        function setSleep(seconds) { sleepSeconds = seconds; dirty = true }
    }
    Component.onCompleted: {
        work.load(profileStore.activeIndex)
        device.startReceiverMonitor()
    }
    Connections {
        target: profileStore
        function onChanged() { if (work.sourceIndex !== profileStore.activeIndex) work.load(profileStore.activeIndex) }
    }
    onClosing: { device.stopStream(); hostEffects.mode = "off" }

    Rail {
        id: rail
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        items: window.pages
        currentIndex: window.currentPage
        onSelected: (index) => window.currentPage = index
    }

    TopBar {
        id: topBar
        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        device: window.device
        profileStore: window.profileStore
        title: window.pages[window.currentPage].label
    }

    StackLayout {
        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: topBar.bottom
        anchors.bottom: parent.bottom
        currentIndex: window.currentPage

        HomePage { device: window.device; work: work; profileStore: window.profileStore; onNavigate: (i) => window.currentPage = i }
        LightingPage { device: window.device; work: work; hostEffects: window.hostEffects }
        KeysPage { device: window.device; work: work }
        MacrosPage { device: window.device; work: work }
        PowerPage { device: window.device; work: work }
        ProfilesPage { device: window.device; work: work; profileStore: window.profileStore }
        DiagnosticsPage { device: window.device; appInfo: window.appInfo }
    }

    // Floating save/apply bar when the working profile has unsaved edits.
    Rectangle {
        visible: work.dirty
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 20
        width: saveRow.implicitWidth + 28
        height: 54
        radius: 27
        color: Theme.surfaceRaised
        border.width: 1
        border.color: Theme.borderStrong
        RowLayout {
            id: saveRow
            anchors.centerIn: parent
            spacing: 10
            Icon { name: "edit"; size: 14; color: Theme.warn }
            Text { text: "Unsaved changes in \"" + work.name + "\""; color: Theme.text; font.pixelSize: 12; font.family: Theme.fontFamily }
            AccentButton { text: "Save profile"; iconName: "save"; primary: false; onClicked: work.save() }
            AccentButton { text: "Save & apply"; iconName: "upload"; enabled: device.verified && !device.busy; onClicked: { work.save(); device.applyProfile(work.snapshot()) } }
        }
    }
}
