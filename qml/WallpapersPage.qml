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
    property bool sidePaneOpen: false
    property bool searchOpen: false

    function toggleSearch() {
        searchOpen = !searchOpen
    }

    // Accessibility: Shortcut for Ctrl+F to toggle search
    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        onActivated: page.toggleSearch()
    }


    header:ToolBar {
        background: Rectangle {
            color: "transparent" //Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
        }
        contentItem: ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            spacing: Kirigami.Units.smallSpacing
        
            RowLayout {
                spacing: Kirigami.Units.smallSpacing

                ToolButton {
                    icon.name: "view-list-details"
                    checkable: true
                    checked: page.sidePaneOpen
                    onClicked: page.sidePaneOpen = checked
                    display: AbstractButton.IconOnly
                    Accessible.name: qsTr("Toggle Folders Panel")
                    ToolTip.text: qsTr("Toggle Folders Panel")
                    ToolTip.visible: hovered
                }
                Item { Layout.fillWidth: true }
                ColumnLayout {
                    id: headerTitle
                    spacing: 0
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        readonly property string fileName: {
                            const parts = page.app.selectedWallpaper.split("/").filter(function (s) { return s.length > 0 })
                            return parts.length > 0 ? parts[parts.length - 1] : ""
                        }
                        text: page.app.selectedWallpaper === "" ? qsTr("No wallpaper selected") : fileName
                        font.bold: true
                        elide: Text.ElideMiddle
                        visible: !page.searchOpen
                    }
                    Label {
                        Layout.fillWidth: true
                        horizontalAlignment: Text.AlignHCenter
                        text: page.app.selectedWallpaperSize
                        color: Kirigami.Theme.disabledTextColor
                        opacity: 0.7
                        visible: page.app.selectedWallpaperSize !== "" && !page.searchOpen
                        //font.pixelSize: Kirigami.Theme.smallFont.pointSize
                    }

                    Kirigami.SearchField {
                        id: searchField
                        Layout.fillWidth: true
                        Layout.preferredWidth: 200
                        placeholderText: qsTr("Filter wallpapers...")
                        onTextChanged: page.app.wallpaperModel.filterText = text
                        visible: page.searchOpen

                        onVisibleChanged: {
                            if (visible) {
                                searchField.forceActiveFocus()
                            }
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                ToolButton {
                    id: searchButton
                    icon.name: "search-symbolic"
                    checkable: true
                    checked: page.searchOpen
                    onClicked: page.toggleSearch()
                    ToolTip.text: qsTr("Toggle Search Panel")
                    ToolTip.visible: hovered
                    display: AbstractButton.IconOnly
                }
            }
            MenuSeparator { Layout.fillWidth: true }
        }
    }

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing

        FolderSidebar {
            id: folderSidebar
            app: page.app
            addFolderDialog: page.addFolderDialog
            Layout.fillHeight: true
            Layout.preferredWidth: page.sidePaneOpen ? 300 : 0
            visible: folderSidebar.Layout.preferredWidth > 0
            opacity: page.sidePaneOpen ? 1 : 0
            clip: true
            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.InOutQuad }
            }
            Behavior on opacity {
                OpacityAnimator { duration: Kirigami.Units.shortDuration }
            }
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ScrollView {
                id: scrollView
                anchors.fill: parent
                clip: true
            
                GridView {
                    id: gridView
                    //anchors.fill: parent
                    anchors.left: parent.left
                    anchors.right: parent.right
                    // Reserve space for the scrollbar to prevent visual overlap
                    anchors.rightMargin: Kirigami.Units.smallSpacing + Kirigami.Units.largeSpacing

                    // Responsive cells, matching the old app: at least two ~250px-wide columns in a 16:9 grid.
                    readonly property int columnCount: Math.max(2, Math.floor(width / 250))
                    cellWidth: Math.floor(width / columnCount)
                    cellHeight: Math.floor(cellWidth * 0.5625)
                    model: page.app.wallpaperModel
                    focus: true
                    highlight: Rectangle {
                        color: Kirigami.Theme.highlightColor
                        //opacity: 0.4
                        radius: Kirigami.Units.smallSpacing
                    }
                    delegate: WallpaperItem {
                        app: page.app
                        grid: gridView
                        // GridView does not resize delegates; fill the cell like the old app.
                        width: gridView.cellWidth
                        height: gridView.cellHeight
                    }
                    onCurrentIndexChanged: page.app.selectWallpaper(
                        gridView.currentIndex >= 0 ? page.app.wallpaperModel.pathAt(gridView.currentIndex) : "")
                    Keys.onReturnPressed: function(event) {
                        event.accepted = true
                        page.app.applyWallpaper()
                    }
                }
            }

            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                visible: page.app.wallpaperModel.count === 0
                icon.name: "folder-images"
                text: searchField.text === "" ? qsTr("No wallpapers in this folder") : qsTr("No matching wallpapers")
                explanation: searchField.text === "" ? qsTr("Pick a folder with images to get started.") : qsTr("Try a different search term.")
            }
        }
    }

    Component.onCompleted: gridView.forceActiveFocus()
}
