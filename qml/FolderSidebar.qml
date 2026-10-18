import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound
Item {
    id: root
    required property AppController app
    required property FolderDialog addFolderDialog

    implicitWidth: 200
    Layout.fillHeight: true

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.mediumSpacing

        RowLayout {
            Kirigami.Heading {
                level: 4
                text: qsTr("Folders")
            }
            Item { Layout.fillWidth: true }
            ToolButton {
                icon.name: "list-add"
                display: AbstractButton.IconOnly
                Accessible.name: qsTr("Add Folder")
                onClicked: root.addFolderDialog.open()
            }
        }

        ListView {
            id: folderList
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: root.app.folders
            highlight: Rectangle {
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.highlightColor
                opacity: 0.25
            }
            delegate: FolderRow {
                app: root.app
            }
        }
    }
}
