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

    highlighted: root.app.currentFolderIndex === root.index
    onClicked: root.app.setCurrentFolderIndex(root.index)

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
                elide: Text.ElideMiddle
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                Layout.fillWidth: true
            }
        }
        ToolButton {
            icon.name: "user-trash"
            display: AbstractButton.IconOnly
            Accessible.name: qsTr("Remove Folder")
            onClicked: root.app.removeFolder(root.index)
        }
    }
}
