import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// One editable color row (label + swatch) for the tool pages' palette
// sections. The Repeater binds the per-item color entry via modelData.
RowLayout {
    id: root

    required property var modelData

    // Emitted with the hex color picked in the swatch dialog.
    signal colorAccepted(string hex)

    Layout.fillWidth: true
    spacing: Kirigami.Units.smallSpacing

    Label {
        text: root.modelData.key
        Layout.fillWidth: true
        Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
        elide: Text.ElideRight
    }

    ColorDot {
        enablePicker: true
        color: root.modelData.display
        onAccepted: (hex) => root.colorAccepted(hex)
    }
}
