import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Rectangle {
    id: root
    required property AppController app
    required property FolderDialog addFolderDialog

    Layout.preferredWidth: 300
    Layout.fillHeight: true
    color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
    radius: Kirigami.Units.largeSpacing
    border.color: Kirigami.Theme.highlightColor
    border.width: 1.5

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.mediumSpacing
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Kirigami.Heading {
                level: 4
                text: qsTr("Folders")
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                display: AbstractButton.IconOnly
                Accessible.name: qsTr("Add Folder")
                onClicked: root.addFolderDialog.open()
                // Kirigami.Icon with a semantic color: the Basic style follows the platform palette,
                // which is light by default and would render the icon nearly invisible.
                contentItem: Kirigami.Icon {
                    source: "folder-new"
                    color: Kirigami.Theme.textColor
                    width: Kirigami.Units.iconSizes.medium
                    height: Kirigami.Units.iconSizes.medium
                }
            }
        }

        MenuSeparator { Layout.fillWidth: true }

        ListView {
            id: folderList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.app.folders
            // highlight: Rectangle {
            //     radius: Kirigami.Units.smallSpacing
            //     color: Kirigami.Theme.highlightColor
            //     opacity: 0.25
            // }
            delegate: FolderRow {
                app: root.app
                // ListView does not stretch delegates; match the list width so the row (and its button) fit the sidebar.
                width: folderList.width
            }
        }
    }
}


