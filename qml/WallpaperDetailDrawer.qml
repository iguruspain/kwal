import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.OverlayDrawer {
    id: drawer

    required property AppController app
    // Absolute path of the double-clicked wallpaper.
    required property string wallpaper
    required property bool isVideo
    // The 15 color categories (name/hex/label) shared with the search chips.
    required property var categorySwatches

    // Manually edited categories/tags of the current wallpaper; kept in sync
    // with WallpapersInfo and written back on every change.
    property var categories: []
    property var tags: []

    property string paletteBackend: "materialyou"
    property bool paletteDark: false
    property var paletteSwatches: []
    property int paletteSelected: -1
    property bool paletteBusy: false
    // True once the delayed video playback has started; the MediaPlayer
    // is destroyed on close or switch, which disarms it again.
    property bool mediaArmed: false

    edge: Qt.RightEdge
    // Non-modal: stays open over the grid while the grid remains
    // operable, so double-clicking another (or the same) cell updates
    // this drawer's video in place instead of closing it.
    modal: false
    // The installed template binds drawerOpen to !modal; with a
    // non-modal drawer that evaluates to true, so pin it closed: the
    // drawer only opens on double-click (WallpapersPage.openDetail).
    drawerOpen: false
    // The template handle is drawn below the overlay in non-modal mode
    // (invisible) and its x binding reads a null parent while the
    // window overlay is not attached, so it is disabled; the content
    // header carries the close button instead.
    handleVisible: false
    // Keep the edge drag-margin disabled so the drawer cannot be opened
    // by dragging the window edge; double-click is the opening gesture.
    dragMargin: 0
    // Escape (and the header X) close the drawer; a release outside it
    // must not close it, so the grid stays operable while it is open.
    closePolicy: Popup.CloseOnEscape
    // qmllint disable unqualified
    preferredSize: applicationWindow() ? Math.round(applicationWindow().width * 0.6) : 480
    // qmllint enable unqualified
    onWallpaperChanged: {
        syncWithInfo()
        mediaArmed = false
    }

    function syncWithInfo() {
        categories = app.wallpapersInfo.colorCategoriesFor(drawer.wallpaper)
        tags = app.wallpapersInfo.tagsFor(drawer.wallpaper)
        paletteSwatches = []
        paletteSelected = -1
    }

    Component.onCompleted: syncWithInfo()

    function hexFor(name) {
        for (const s of categorySwatches) {
            if (s.name === name) {
                return s.hex
            }
        }
        return Kirigami.Theme.highlightColor
    }

    // QML split() returns a plain JS array (no last()); index the last part.
    function fileNameOf(path) {
        const parts = path.split("/")
        return parts[parts.length - 1]
    }

    Connections {
        target: drawer.app.wallpapersInfo
        function onEntryChanged(path) {
            if (path === drawer.wallpaper) {
                drawer.syncWithInfo()
            }
        }
    }

    Connections {
        target: drawer.app.paletteExtractor
        function onPaletteReady(path, colors, accents, seed) {
            if (path === drawer.wallpaper) {
                drawer.paletteBusy = false
                drawer.paletteSwatches = colors
                drawer.paletteSelected = -1
            }
        }
        function onExtractionFailed(path, message) {
            if (path === drawer.wallpaper) {
                drawer.paletteBusy = false
                // qmllint disable unqualified
                const win = applicationWindow()
                // qmllint enable unqualified
                if (win)
                    win.showPassiveNotification(message, "long")
            }
        }
    }

    // Invisible TextEdit used to copy a swatch hex to the clipboard.
    TextEdit {
        id: clipboard
        visible: false
    }

    function copySwatchToClipboard(hex) {
        clipboard.text = hex
        clipboard.selectAll()
        // qmllint disable unqualified
        const win = applicationWindow()
        // qmllint enable unqualified
        if (win)
            win.showPassiveNotification(qsTr("Copied %1").arg(hex), "short")
    }

    contentItem: Flickable {
        id: scroller
        contentWidth: content.width
        contentHeight: content.height
        boundsBehavior: Flickable.StopAtBounds

        ColumnLayout {
            id: content
            width: scroller.width
            spacing: Kirigami.Units.largeSpacing

            // Media area: the base layer is the wallpaper itself
            // (images) or the cached first frame (videos), always
            // shown; the live playback starts on top of it after a
            // short delay, so the first frame is visible before the
            // decoder spins up.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 272
                clip: true

                Image {
                    id: mediaImage
                    anchors.fill: parent
                    fillMode: Image.PreserveAspectCrop
                    asynchronous: true
                    smooth: true
                    source: drawer.isVideo
                        ? (drawer.app.wallpapersInfo.firstFrameFor(drawer.wallpaper) !== ""
                            ? drawer.app.fileUrl(drawer.app.wallpapersInfo.firstFrameFor(drawer.wallpaper))
                            : "")
                        : drawer.app.fileUrl(drawer.wallpaper)
                }

                // Live playback, the same "armed" pattern as the grid
                // cells: starts only after a delay while the drawer is
                // open; destroyed (not just paused) on close or switch,
                // so the grid cell's video resumes its loop.
                Loader {
                    id: videoLoader
                    anchors.fill: parent
                    active: drawer.isVideo && drawer.drawerOpen && drawer.mediaArmed
                    onActiveChanged: if (!active) drawer.mediaArmed = false
                    sourceComponent: Component {
                        Item {
                            VideoOutput {
                                id: videoSurface
                                anchors.fill: parent
                                fillMode: VideoOutput.PreserveAspectCrop
                            }
                            MediaPlayer {
                                source: drawer.app.fileUrl(drawer.wallpaper)
                                autoPlay: true
                                loops: MediaPlayer.Infinite
                                activeAudioTrack: -1
                                videoOutput: videoSurface
                            }
                        }
                    }
                }

                Timer {
                    id: mediaStartTimer
                    interval: 250
                    running: drawer.isVideo && drawer.drawerOpen && !drawer.mediaArmed
                    onTriggered: drawer.mediaArmed = true
                }
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Kirigami.Heading {
                    Layout.fillWidth: true
                    level: 3
                    wrapMode: Text.WordWrap
                    text: drawer.fileNameOf(drawer.wallpaper)
                }

                ToolButton {
                    icon.name: "dialog-close"
                    display: AbstractButton.IconOnly
                    Accessible.name: qsTr("Close")
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: drawer.drawerOpen = false
                }
            }

            // Category chips; the "Add" menu offers the missing categories.
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: drawer.categories
                    delegate: Kirigami.Chip {
                        required property string modelData
                        Layout.preferredWidth: implicitWidth
                        width: implicitWidth
                        text: modelData
                        icon.name: "tag"
                        icon.color: drawer.hexFor(modelData)
                        onRemoved: {
                            const c = drawer.categories.filter(c2 => c2 !== modelData)
                            drawer.categories = c
                            drawer.app.wallpapersInfo.setCategories(drawer.wallpaper, c)
                        }
                    }
                }

                ToolButton {
                    Layout.alignment: Qt.AlignRight
                    icon.name: "list-add"
                    Accessible.name: qsTr("Add category")
                    ToolTip.text: qsTr("Add category")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: addMenu.open()
                }
                Menu {
                    id: addMenu
                    Instantiator {
                        id: addCategoryInstantiator
                        model: drawer.categorySwatches.filter(s => !drawer.categories.includes(s.name))
                        delegate: MenuItem {
                            required property var modelData
                            text: modelData.label
                            icon.name: "tag"
                            icon.color: modelData.hex
                            onTriggered: {
                                const list = drawer.categories.slice()
                                list.push(modelData.name)
                                drawer.categories = list
                                drawer.app.wallpapersInfo.setCategories(drawer.wallpaper, list)
                            }
                        }
                        onObjectAdded: (index, object) => addMenu.insertItem(index, object)
                        onObjectRemoved: (index, object) => addMenu.removeItem(object)
                    }
                }
            }

            // Tags: existing ones as chips, new ones via the Enter-accepted field.
            ColumnLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: drawer.tags
                    delegate: Kirigami.Chip {
                        required property string modelData
                        Layout.preferredWidth: implicitWidth
                        width: implicitWidth
                        text: modelData
                        onRemoved: {
                            const t = drawer.tags.filter(t2 => t2 !== modelData)
                            drawer.tags = t
                            drawer.app.wallpapersInfo.setTags(drawer.wallpaper, t)
                        }
                    }
                }

                TextField {
                    id: tagField
                    Layout.fillWidth: true
                    placeholderText: qsTr("Add tag...")
                    onAccepted: {
                        const t = tagField.text.trim()
                        if (t !== "" && !drawer.tags.includes(t)) {
                            const list = drawer.tags.slice()
                            list.push(t)
                            drawer.tags = list
                            drawer.app.wallpapersInfo.setTags(drawer.wallpaper, list)
                        }
                        tagField.text = ""
                    }
                }
            }

            // Palette section: backend, dark mode (Material You), extraction.
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                ComboBox {
                    id: backendCombo
                    Layout.preferredWidth: 160
                    model: [qsTr("Material You"), qsTr("ImageMagick")]
                    onActivated: {
                        drawer.paletteBackend = currentIndex === 1 ? "imagemagick" : "materialyou"
                        drawer.paletteSwatches = []
                        drawer.paletteSelected = -1
                    }
                }

                Switch {
                    id: darkSwitch
                    checked: drawer.paletteDark
                    enabled: drawer.paletteBackend === "materialyou"
                    onToggled: drawer.paletteDark = checked
                    Accessible.name: qsTr("Dark")
                }
                Kirigami.Heading {
                    level: 4
                    text: qsTr("Dark")
                    opacity: darkSwitch.enabled ? 1.0 : 0.5
                }

                ToolButton {
                    Layout.alignment: Qt.AlignRight
                    enabled: !drawer.paletteBusy
                    icon.name: "color-palette"
                    Accessible.name: qsTr("Extract palette")
                    ToolTip.text: qsTr("Extract palette")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: {
                        drawer.paletteBusy = true
                        drawer.app.paletteExtractor.extract(drawer.wallpaper, drawer.paletteBackend, drawer.paletteDark)
                    }
                }
            }

            // Swatches: left click selects, right click copies the hex.
            Grid {
                Layout.fillWidth: true
                columns: 6
                columnSpacing: Kirigami.Units.smallSpacing
                rowSpacing: Kirigami.Units.smallSpacing

                Repeater {
                    model: drawer.paletteSwatches
                    delegate: Rectangle {
                        required property string modelData
                        required property int index
                        id: swatch
                        implicitWidth: Kirigami.Units.gridUnit * 2
                        implicitHeight: implicitWidth
                        width: implicitWidth
                        height: implicitHeight
                        radius: Kirigami.Units.smallSpacing
                        color: modelData
                        border.width: drawer.paletteSelected === index ? 2 : 1
                        border.color: drawer.paletteSelected === index
                            ? Kirigami.Theme.highlightColor
                            : Kirigami.Theme.disabledTextColor
                        ToolTip.text: modelData.toUpperCase()
                        ToolTip.visible: swatchMouse.containsMouse
                        ToolTip.delay: Kirigami.Units.toolTipDelay

                        MouseArea {
                            id: swatchMouse
                            anchors.fill: parent
                            hoverEnabled: true
                            acceptedButtons: Qt.LeftButton | Qt.RightButton
                            onClicked: (mouse) => {
                                if (mouse.button === Qt.RightButton) {
                                    drawer.copySwatchToClipboard(swatch.modelData)
                                } else {
                                    drawer.paletteSelected = swatch.index
                                }
                                mouse.accepted = true
                            }
                        }
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    Layout.alignment: Qt.AlignRight
                    text: qsTr("Set as wallpaper")
                    icon.name: "preferences-desktop-wallpaper"
                    onClicked: drawer.app.applyWallpaper()
                }
            }
        }
    }
}
