import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// A section with an arrow ToolButton + bold label header (same pattern as
// StarshipPage's "Collapse palette") that toggles its content. The content
// stays alive while collapsed (only hidden) so its children keep their
// state (typed text, selection) across toggles.
ColumnLayout {
    id: root

    property string title: ""
    property bool open: true
    property Component sectionContent: Component { Item {} }

    spacing: Kirigami.Units.smallSpacing

    RowLayout {
        Layout.fillWidth: true
        spacing: Kirigami.Units.smallSpacing
        ToolButton {
            icon.name: root.open ? "arrow-down" : "arrow-right"
            onClicked: root.open = !root.open
            ToolTip.text: root.open ? qsTr("Collapse %1").arg(root.title) : qsTr("Expand %1").arg(root.title)
            ToolTip.visible: hovered
        }
        Label {
            text: root.title
            font.bold: true
        }
        Item { Layout.fillWidth: true }
    }

    Kirigami.Separator {
        Layout.fillWidth: true
    }

    Loader {
        visible: root.open
        sourceComponent: root.sectionContent
        Layout.fillWidth: true
    }
}
