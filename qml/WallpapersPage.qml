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
    // Gates video playback: only the current tab plays.
    property bool pageActive: true
    property bool searchOpen: false
    property bool detailOpen: false
    property real detailPanelFraction: 0.4

    // Chips filter the grid with AND semantics.
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
        // A hidden field orphans focus: hand it back to the grid so arrow keys keep working.
        if (!searchOpen)
            gridView.forceActiveFocus()
    }

    Shortcut {
        sequence: "Ctrl+F"
        context: Qt.ApplicationShortcut
        onActivated: page.toggleSearch()
    }

    // Escape closes the topmost layer (panel first, then search) and returns focus to the grid.
    Shortcut {
        sequence: "Escape"
        context: Qt.ApplicationShortcut
        enabled: page.detailOpen || page.searchOpen
        onActivated: {
            if (page.detailOpen)
                page.detailOpen = false
            else
                page.searchOpen = false
            gridView.forceActiveFocus()
        }
    }

    header:ToolBar {
        background: Rectangle {
            color: "transparent"
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
                    Accessible.name: qsTr("Toggle Search Panel")
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
                        // Mouse-only: the chips stay out of the Tab chain (Clear keeps its stop).
                        activeFocusOnTab: false
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
        id: contentRow
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
                    anchors.left: parent.left
                    anchors.right: parent.right
                    // Reserve space for the scrollbar to prevent visual overlap
                    anchors.rightMargin: Kirigami.Units.smallSpacing + Kirigami.Units.largeSpacing

                    // One ~250 px 16:9 column per 250 px of width; single full-width column when squeezed.
                    readonly property int columnCount: Math.max(1, Math.floor(width / 250))
                    cellWidth: Math.floor(width / columnCount)
                    cellHeight: Math.floor(cellWidth * 0.5625)
                    // WallpaperItem is stateless per model row, so the delegate pool stays bounded; cacheBuffer smooths pop-in.
                    reuseItems: true
                    cacheBuffer: 700
                    model: page.app.wallpaperModel
                    focus: true
                    // The view is the grid's single tab stop (views default activeFocusOnTab false); delegates stay non-focusable, arrows move the highlight.
                    activeFocusOnTab: true
                    highlight: Rectangle {
                        color: Kirigami.Theme.highlightColor
                        radius: Kirigami.Units.smallSpacing
                        opacity: gridView.activeFocus ? 1.0 : 0.4
                    }
                    delegate: WallpaperItem {
                        required property var model
                        app: page.app
                        grid: gridView
                        pageVisible: page.pageActive
                        detailOpen: page.detailOpen
                        isAnimated: model.isAnimated
                        sourceColors: model.sourceColors
                        onOpenDetail: page.openDetail()
                        // GridView does not resize delegates; fill the cell explicitly.
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
                    onCurrentIndexChanged: {
                        // Selection is the wallpaper path, not the row number: an index with no row behind it (filtered out, folder switch) must leave the selection untouched.
                        const path = gridView.currentIndex >= 0
                            ? page.app.wallpaperModel.pathAt(gridView.currentIndex)
                            : ""
                        if (path !== "")
                            page.app.selectWallpaper(path)
                    }
                    Keys.onReturnPressed: function(event) {
                        event.accepted = true
                        page.app.applyWallpaper()
                    }
                    Keys.onSpacePressed: function(event) {
                        event.accepted = true
                        if (page.app.selectedWallpaper !== "")
                            page.openDetail()
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

        // Not an overlay; the animated Layout width doubles as the visibility driver (0 = hidden).
        WallpaperDetailDrawer {
            id: detailPanel
            app: page.app
            wallpaper: page.app.selectedWallpaper
            isVideo: page.app.wallpaperModel.isVideoFile(page.app.selectedWallpaper)
            categorySwatches: page.categorySwatches
            open: page.detailOpen
            Layout.fillHeight: true
            Layout.preferredWidth: page.detailOpen ? contentRow.width * page.detailPanelFraction : 0
            visible: detailPanel.Layout.preferredWidth > 0
            opacity: page.detailOpen ? 1 : 0
            clip: true
            Behavior on Layout.preferredWidth {
                NumberAnimation { duration: Kirigami.Units.shortDuration; easing.type: Easing.InOutQuad }
            }
            Behavior on opacity {
                OpacityAnimator { duration: Kirigami.Units.shortDuration }
            }
            onCloseRequested: {
                page.detailOpen = false
                // The drawer's controls vanish; keep focus with the grid.
                gridView.forceActiveFocus()
            }
        }
    }

    // The panel closes only when the selection itself is cleared (see AppController::applyWallpaper).
    function openDetail() {
        page.detailOpen = true
    }

    Connections {
        target: page.app
        function onSelectedWallpaperChanged(path) {
            if (path === "")
                page.detailOpen = false
        }
    }

    // A model reset keeps GridView's numeric cursor, which would name a different file: re-derive currentIndex from the selected path. A selection the filter hides leaves the cursor at -1; the selection itself stays.
    Connections {
        target: page.app.wallpaperModel
        function onModelReset() {
            gridView.currentIndex = page.app.wallpaperModel.rowForPath(page.app.selectedWallpaper)
        }
    }

    Component.onCompleted: {
        // Position at the last-applied wallpaper (kwalrc [Wallpapers] currentWallpaper) if present; currentIndex also selects.
        const index = page.app.wallpaperModel.rowForPath(page.app.currentWallpaper)
        if (index >= 0) {
            gridView.currentIndex = index
            gridView.positionViewAtIndex(index, GridView.Center)
        }
        gridView.forceActiveFocus()
    }
}
