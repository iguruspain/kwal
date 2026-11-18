import QtQuick
import QtQuick.Controls

// fps 0 displays as "current" (keep the source frame rate); the C++ setter clamps typed values and re-syncs via fpsChanged.
ComboBox {
    id: root

    editable: true

    property int fps: 0

    model: [qsTr("current"), "24fps", "30fps", "60fps"]

    function displayValue(v) {
        return v === 0 ? qsTr("current") : v + "fps"
    }

    // Editable ComboBox: typed text is editText; a dropdown selection leaves editText empty and puts the item in currentText.
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
