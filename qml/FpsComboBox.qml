import QtQuick
import QtQuick.Controls

// Editable frame-rate picker: the presets are "current" (0 = keep the
// source frame rate) plus fixed values; typing another number keeps it
// as a custom value. Out-of-range entries are clamped by the C++
// setter, which re-syncs the display through fpsChanged.
ComboBox {
    id: root

    editable: true

    property int fps: 0

    model: [qsTr("current"), "24fps", "30fps", "60fps"]

    function displayValue(v) {
        return v === 0 ? qsTr("current") : v + "fps"
    }

    // The typed text lives in editText; a dropdown selection leaves the
    // field empty and currentText holds the item text.
    function fieldText() {
        return root.editText === "" ? root.currentText : root.editText
    }

    function parseText(t) {
        t = t.trim().toLowerCase()
        if (t === "" || t === qsTr("current")) {
            return 0
        }
        const n = parseInt(t.replace(/fps$/, ""), 10)
        return isNaN(n) ? -1 : n
    }

    function sync() {
        const i = model.indexOf(displayValue(fps))
        if (i >= 0) {
            root.currentIndex = i
        } else {
            root.currentIndex = -1
            root.editText = displayValue(fps)
        }
    }

    Component.onCompleted: sync()
    onFpsChanged: sync()

    onActivated: {
        const v = parseText(fieldText())
        if (v >= 0) {
            fps = v
        } else {
            sync()
        }
    }

    onAccepted: {
        const v = parseText(fieldText())
        if (v >= 0) {
            fps = v
        } else {
            sync()
        }
    }
}
