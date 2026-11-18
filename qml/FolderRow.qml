import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ItemDelegate {
    id: root
    required property AppController app
    required property string modelData
    required property int index

    readonly property string displayName: {
        const parts = modelData.split("/").filter(function (s) { return s.length > 0 })
        return parts.length > 0 ? parts[parts.length - 1] : modelData
    }

    // No ListView highlight: this background draws hover/press/selection; currentIndex is the applied folder, arrows move it, Enter/Space applies.
    readonly property bool cursorOnRow: ListView.view && ListView.view.currentIndex === index
    background: Rectangle {
        radius: Kirigami.Units.smallSpacing
        border.width: root.cursorOnRow ? 2 : 0
        border.color: Kirigami.Theme.highlightColor
        color: root.cursorOnRow ? Qt.alpha(Kirigami.Theme.highlightColor, 0.8)
             : root.down ? Qt.alpha(Kirigami.Theme.highlightColor, 0.4)
             : root.hovered ? Qt.alpha(Kirigami.Theme.highlightColor, 0.25)
             : "transparent"
    }
    onClicked: root.app.setCurrentFolderIndex(root.index)
    // ItemDelegate swallows Space as its own click; the attached handler runs first and applies this folder.
    Keys.onSpacePressed: function(event) {
        event.accepted = true
        root.app.setCurrentFolderIndex(root.index)
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.smallSpacing

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.gridUnit / 4
            Label {
                text: root.displayName
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Label {
                text: root.modelData
                elide: Text.ElideLeft
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                Layout.fillWidth: true
            }
        }
        ToolButton {
            icon.name: "edit-clear"
            display: AbstractButton.IconOnly
            Accessible.name: qsTr("Remove Folder")
            hoverEnabled: true
            ToolTip.text: qsTr("Remove this folder")
            ToolTip.visible: hovered
            onClicked: root.app.removeFolder(root.index)
            // Not a tab stop: hidden until hover; the sidebar list is the single stop.
            activeFocusOnTab: false
        }
    }
}
