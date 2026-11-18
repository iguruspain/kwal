import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// Shared swatch: enablePicker + accepted() for tool pages; plain clicked() with right-click hex copy (enableCopy) for grid/drawer; filter chips set enableCopy: false + tooltipText.
Item {
    id: root

    // "#rrggbb", "#aarrggbb" (alpha first), or "transparent".
    property string color: "transparent"
    property bool selected: false
    property bool enablePicker: false
    property bool enableCopy: true
    property string tooltipText: ""
    property real dotSize: Kirigami.Units.gridUnit * 1.5

    // Emitted on left-click only when the picker is disabled.
    signal clicked()
    signal accepted(string hex)

    implicitWidth: dotSize
    implicitHeight: dotSize
    Accessible.role: Accessible.Button
    Accessible.name: root.tooltipText !== ""
        ? root.tooltipText
        : root.color.startsWith("#")
            ? root.color.toUpperCase()
            : qsTr("Pick color")

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.smallSpacing
        color: root.color.startsWith("#")
            ? root.color
            : "transparent"
        border.color: root.selected
            ? Kirigami.Theme.positiveTextColor
            : Kirigami.Theme.textColor
        border.width: root.selected ? 2 : 1

        ToolTip.text: root.tooltipText !== ""
            ? root.tooltipText
            : root.color.startsWith("#")
                ? root.color.toUpperCase()
                : qsTr("Pick color")
        ToolTip.visible: mouseArea.containsMouse
        ToolTip.delay: Kirigami.Units.toolTipDelay

        MouseArea {
            id: mouseArea
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            onClicked: (mouse) => {
                if (mouse.button === Qt.RightButton) {
                    if (root.enableCopy)
                        root.copyToClipboard()
                } else if (root.enablePicker) {
                    root.openDialog()
                } else {
                    root.clicked()
                }
                mouse.accepted = true
            }
        }
    }

    ColorDialog {
        id: colorDialog
        onAccepted: root.accepted(root.dialogHex())
    }

    // Clipboard plumbing; Accessible.ignored keeps it out of the a11y tree as a phantom editable field.
    TextEdit {
        id: clipboard
        visible: false
        Accessible.ignored: true
    }

    function dialogHex() {
        const c = colorDialog.selectedColor
        const a = Math.round(c.a * 255)
        let hex = a < 255
            ? a.toString(16).padStart(2, "0")
            : ""
        hex += Math.round(c.r * 255).toString(16).padStart(2, "0")
        hex += Math.round(c.g * 255).toString(16).padStart(2, "0")
        hex += Math.round(c.b * 255).toString(16).padStart(2, "0")
        return "#" + hex
    }

    function openDialog() {
        if (root.color.startsWith("#")) {
            const hex = root.color.substring(1)
            colorDialog.selectedColor = hex.length === 8
                ? Qt.rgba(parseInt(hex.substring(2, 4), 16) / 255,
                           parseInt(hex.substring(4, 6), 16) / 255,
                           parseInt(hex.substring(6, 8), 16) / 255,
                           parseInt(hex.substring(0, 2), 16) / 255)
                : Qt.rgba(parseInt(hex.substring(0, 2), 16) / 255,
                           parseInt(hex.substring(2, 4), 16) / 255,
                           parseInt(hex.substring(4, 6), 16) / 255, 1)
        }
        colorDialog.open()
    }

    function copyToClipboard() {
        if (!root.color.startsWith("#"))
            return
        clipboard.text = root.color
        clipboard.selectAll()
        clipboard.copy()
        // qmllint disable unqualified
        const win = applicationWindow()
        // qmllint enable unqualified
        if (win)
            win.showPassiveNotification(
                qsTr("Copied: %1").arg(root.color), "short")
    }
}
