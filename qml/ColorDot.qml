import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// Color swatch button shared by the tool pages: left-click opens the
// built-in color dialog, right-click copies the current color to the
// clipboard.
Item {
    id: root

    // Current color: "#rrggbb", "#aarrggbb", or "transparent".
    property string color: "transparent"

    // Emitted with the hex color picked in the dialog.
    signal accepted(string hex)

    implicitWidth: Kirigami.Units.gridUnit * 1.5
    implicitHeight: Kirigami.Units.gridUnit * 1.5

    Rectangle {
        anchors.fill: parent
        radius: Kirigami.Units.smallSpacing
        color: root.color.startsWith("#")
            ? root.color
            : "transparent"
        border.color: Kirigami.Theme.highlightColor //Kirigami.Theme.disabledTextColor
        border.width: 1.5

        ToolTip.text: root.color.startsWith("#")
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
                if (mouse.button === Qt.RightButton)
                    root.copyToClipboard()
                else
                    root.openDialog()
                mouse.accepted = true
            }
        }
    }

    ColorDialog {
        id: colorDialog
        onAccepted: root.accepted(root.dialogHex())
    }

    // Invisible TextEdit used to copy text to the clipboard.
    TextEdit {
        id: clipboard
        visible: false
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
