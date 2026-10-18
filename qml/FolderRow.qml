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
                elide: Text.ElideLeft
                color: Kirigami.Theme.disabledTextColor
                font: Kirigami.Theme.smallFont
                Layout.fillWidth: true
            }
        }
        ToolButton {
            display: AbstractButton.IconOnly
            Accessible.name: qsTr("Remove Folder")
            hoverEnabled: true
            ToolTip.text: qsTr("Remove this folder")
            ToolTip.visible: hovered
            onClicked: root.app.removeFolder(root.index)
            // Kirigami.Icon with a semantic color: the Basic style follows the platform palette,
            // which is light by default and would render the icon nearly invisible.
            contentItem: Kirigami.Icon {
                source: "edit-delete"
                color: Kirigami.Theme.textColor
                width: Kirigami.Units.iconSizes.medium
                height: Kirigami.Units.iconSizes.medium
            }
        }
    }
}
