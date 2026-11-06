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
            // The list itself is the sidebar's tab stop (rows are
            // ItemDelegates, which are not stops): Tab enters here, arrow
            // keys move the cursor, Enter/Space picks the folder under it.
            activeFocusOnTab: true
            // No ListView highlight rectangle: the selection/cursor fill is
            // drawn by FolderRow's own background, guaranteeing identical
            // geometry to the hover state.
            // Keep the keyboard cursor on the active folder, so Tab into the
            // list starts where the user is; arrow keys move the cursor
            // without applying until Enter.
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
            // The keyboard's "click" for the row under the cursor.
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


