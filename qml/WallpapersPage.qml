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
    // True while this page is the current tab; drives video playback gating.
    property bool pageActive: true
    property bool searchOpen: false

    // The 15 matugen color categories (name/hex/label from the original
    // app's categoryColorModel); the chips filter the grid (AND semantics).
    readonly property var categorySwatches: [
        { name: "red", hex: "#E53935", label: qsTr("Red") },
        { name: "orange", hex: "#FB8C00", label: qsTr("Orange") },
        { name: "yellow", hex: "#FDD835", label: qsTr("Yellow") },
        { name: "yellow-green", hex: "#7CB342", label: qsTr("Yellow-Green") },
        { name: "green", hex: "#43A047", label: qsTr("Green") },
        { name: "cyan-green", hex: "#00897B", label: qsTr("Cyan-Green") },
        { name: "cyan", hex: "#00ACC1", label: qsTr("Cyan") },
        { name: "blue-cyan", hex: "#1E88E5", label: qsTr("Blue-Cyan") },
        { name: "blue", hex: "#3949AB", label: qsTr("Blue") },
        { name: "violet", hex: "#8E24AA", label: qsTr("Violet") },
        { name: "magenta", hex: "#D81B60", label: qsTr("Magenta") },
        { name: "rose", hex: "#F06292", label: qsTr("Rose") },
        { name: "black", hex: "#212121", label: qsTr("Black/Dark") },
        { name: "gray", hex: "#757575", label: qsTr("Gray") },
        { name: "white", hex: "#F5F5F5", label: qsTr("White/Light") }
    ]
    readonly property var selectedCategories: page.app.wallpaperModel.categoryFilter

    function toggleCategory(name) {
        const selected = page.selectedCategories.slice()
        const index = selected.indexOf(name)
        if (index === -1) {
            selected.push(name)
        } else {
            selected.splice(index, 1)
        }
        page.app.wallpaperModel.setCategoryFilter(selected)
    }

    function clearCategories() {
        page.app.wallpaperModel.setCategoryFilter([])
    }

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
                    Layout.maximumWidth: 600

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
            RowLayout {
                id: categoryChips
                spacing: Kirigami.Units.smallSpacing
                visible: page.searchOpen && page.app.colorCategoriesEnabled
                Item { Layout.fillWidth: true }
                Repeater {
                    model: page.categorySwatches
                    delegate: ColorDot {
                        required property var modelData
                        id: categorySwatch
                        color: modelData.hex
                        tooltipText: modelData.label
                        enableCopy: false
                        selected: page.selectedCategories.indexOf(modelData.name) !== -1
                        onClicked: page.toggleCategory(modelData.name)
                    }
                }
                Kirigami.Chip {
                    text: qsTr("Clear")
                    visible: page.selectedCategories.length > 0
                    onClicked: page.clearCategories()
                }
                Item { Layout.fillWidth: true }
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
                        required property var model
                        app: page.app
                        grid: gridView
                        pageVisible: page.pageActive
                        detailOpen: detailDrawerLoader.active
                            ? (detailDrawerLoader.item as WallpaperDetailDrawer).drawerOpen
                            : false
                        sourceColors: model.sourceColors
                        onOpenDetail: (path, video) => page.openDetail(path, video)
                        // GridView does not resize delegates; fill the cell like the old app.
                        width: gridView.cellWidth
                        height: gridView.cellHeight

                        Rectangle {
                        id: thumbBorder
                            anchors.fill: parent
                            anchors.margins: 4
                            color: "transparent"
                            border.color: Qt.alpha(Kirigami.Theme.textColor, 0.30)
                            border.width: 2
                            radius: Kirigami.Units.smallSpacing
                            z: 10
                        }
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

    // Detail drawer opened by double-clicking a cell. Created lazily on
    // first open: Kirigami 6.31's DrawerHandle template declares
    // `parent: overlay?.parent ?? null` and its `x` binding reads
    // `parent.width` without a null guard, so a handle created before the
    // window's overlay exists logs
    // "TypeError: Cannot read property 'width' of null" at startup.
    // Creating the drawer on first double-click (window fully up by then)
    // makes that state unreachable.
    Component {
        id: detailDrawerComponent
        WallpaperDetailDrawer {
            app: page.app
            parent: page
            wallpaper: ""
            isVideo: false
            categorySwatches: page.categorySwatches
        }
    }
    Loader {
        id: detailDrawerLoader
        active: false
        sourceComponent: detailDrawerComponent
    }

    function openDetail(path, video) {
        detailDrawerLoader.active = true
        const drawer = detailDrawerLoader.item as WallpaperDetailDrawer
        drawer.wallpaper = path
        drawer.isVideo = video
        drawer.drawerOpen = true
    }

    Component.onCompleted: gridView.forceActiveFocus()
}
