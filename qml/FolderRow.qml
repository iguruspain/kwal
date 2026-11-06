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

    // Selection + keyboard cursor drawn here (no ListView highlight): the
    // background fills the delegate, so hover/press/selection share one
    // geometry. The ListView keeps currentIndex on the applied folder;
    // arrow keys move it before Enter/Space applies it.
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
    // ItemDelegate (AbstractButton) swallows Space as its own click; the
    // attached handler runs first and applies the row's folder directly.
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
            // Not a tab stop: it is hidden until hover, so tabbing to it
            // would land focus on an invisible control. The sidebar itself
            // is the single stop of the folder list.
            activeFocusOnTab: false
        }
    }
}
