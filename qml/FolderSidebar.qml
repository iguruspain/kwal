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
    color: Qt.alpha(Kirigami.Theme.backgroundColor, app.transparencyLevel / 100.0)
    radius: Kirigami.Units.largeSpacing
    border.color: Kirigami.Theme.highlightColor
    border.width: 1.5

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.gridUnit
        spacing: Kirigami.Units.gridUnit

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
                // Semantic color: the Basic style's platform-palette icon would be near-invisible.
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
            // The list is the sidebar's single tab stop (ItemDelegate rows are not): Tab enters, arrows move, Enter/Space picks.
            activeFocusOnTab: true
            // Keep currentIndex on the applied folder so Tab lands where the user is; arrows move it without applying.
            Connections {
                target: root.app
                function onCurrentFolderIndexChanged() {
                    folderList.currentIndex = root.app.currentFolderIndex
                }
                function onFoldersChanged() {
                    folderList.currentIndex = root.app.currentFolderIndex
                }
            }
            Component.onCompleted: folderList.currentIndex = root.app.currentFolderIndex
            function activateCurrent() {
                if (folderList.currentIndex >= 0)
                    root.app.setCurrentFolderIndex(folderList.currentIndex)
            }
            Keys.onReturnPressed: function(event) {
                event.accepted = true
                folderList.activateCurrent()
            }
            Keys.onEnterPressed: function(event) {
                event.accepted = true
                folderList.activateCurrent()
            }
            Keys.onSpacePressed: function(event) {
                event.accepted = true
                folderList.activateCurrent()
            }
            delegate: FolderRow {
                app: root.app
                // ListView does not stretch delegates; match the list width so the row (and its button) fit the sidebar.
                width: folderList.width
            }
        }
    }
}


