import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.Page {
    id: page
    title: qsTr("Wallpapers")
    property AppController app
    property FolderDialog addFolderDialog


    header: ToolBar {
        contentItem: RowLayout {
            spacing: Kirigami.Units.smallSpacing

            Label {
                readonly property string fileName: {
                    const parts = page.app.currentWallpaper.split("/").filter(function (s) { return s.length > 0 })
                    return parts.length > 0 ? parts[parts.length - 1] : ""
                }
                text: page.app.currentWallpaper === "" ? qsTr("No wallpaper selected") : fileName
                font.weight: Font.DemiBold
                elide: Text.ElideMiddle
                Layout.fillWidth: true
            }
            Label {
                text: page.app.currentWallpaperSize
                opacity: 0.7
                visible: page.app.currentWallpaperSize !== ""
            }
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        FolderSidebar {
            app: page.app
            addFolderDialog: page.addFolderDialog
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            GridView {
                id: gridView
                anchors.fill: parent
                cellWidth: 176
                cellHeight: 176
                model: page.app.wallpaperModel
                focus: true
                highlight: Rectangle {
                    color: Kirigami.Theme.highlightColor
                    opacity: 0.4
                    radius: Kirigami.Units.smallSpacing
                }
                delegate: WallpaperItem {
                    app: page.app
                }
                Keys.onReturnPressed: function(event) {
                    event.accepted = true
                    page.app.setWallpaper(page.app.wallpaperModel.pathAt(gridView.currentIndex))
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                visible: page.app.wallpaperModel.count === 0
                icon.name: "folder-images"
                text: qsTr("No wallpapers in this folder")
                explanation: qsTr("Pick a folder with images to get started.")
            }
        }
    }

    Component.onCompleted: gridView.forceActiveFocus()
}
