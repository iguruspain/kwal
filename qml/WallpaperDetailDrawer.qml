import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.OverlayDrawer {
    id: drawer

    required property AppController app
    // Absolute path of the wallpaper shown in the drawer. Changing it
    // reloads categories/tags and resets the palette and video state.
    required property string wallpaper
    required property bool isVideo
    // The 15 color categories (name/hex/label) shared with the search chips.
    required property var categorySwatches

    // Local copies of the wallpaper's categories/tags. They are reassigned
    // (never mutated in place) so bindings re-evaluate, and each change is
    // written back through WallpapersInfo.
    property var categories: []
    property var tags: []
    // Whether the tag input field is shown; toggled by the "+" button.
    property bool tagInputVisible: false

    property string paletteBackend: "materialyou"
    property bool paletteDark: true
    property string paletteScheme: "TonalSpot"
    property var paletteSwatches: []
    property int paletteSelected: -1
    property bool paletteBusy: false
    // Set by a short timer after opening so the first frame shows before
    // the video decoder starts. Reset whenever the video Loader unloads.
    property bool mediaArmed: false

    // The page selector index: 0 = Info, 1 = Commands.
    property int pageSelected: 0

    // modal = false;
    // collapsible = false;
    // collapsed = false;

    edge: Qt.RightEdge
    modal: true
    // modal: false
    collapsible: false
    collapsed: false
    drawerOpen: false
    handleVisible: true
    dragMargin: 0
    closePolicy: Popup.CloseOnEscape
    // qmllint disable unqualified
    preferredSize: applicationWindow() ? Math.round(applicationWindow().width * 0.6) : 400
    // qmllint enable unqualified

    onWallpaperChanged: {
        syncWithInfo()
        mediaArmed = false
        pageSelected = 0
    }

    // The drawer always opens on the Info page.
    onDrawerOpenChanged: if (drawerOpen)
        pageSelected = 0

    // Reloads the manual categories/tags from the backend and clears any
    // palette computed for the previous wallpaper.
    function syncWithInfo() {
        categories = app.wallpapersInfo.colorCategoriesFor(drawer.wallpaper)
        tags = app.wallpapersInfo.tagsFor(drawer.wallpaper)
        paletteSwatches = []
        paletteSelected = -1
        // A completion that is about to be discarded must not leave the
        // extract button disabled on the new wallpaper.
        paletteBusy = false
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

    // QML split() returns a plain JS array (no last()), so index the end.
    function fileNameOf(path) {
        const parts = path.split("/")
        return parts[parts.length - 1]
    }

    // Keeps the drawer in sync when the entry is modified elsewhere
    // (e.g. a rescan or an edit from the grid).
    Connections {
        target: drawer.app.wallpapersInfo
        function onEntryChanged(path) {
            if (path === drawer.wallpaper) {
                drawer.syncWithInfo()
            }
        }
    }

    // Extraction is asynchronous and results for other wallpapers can
    // arrive late, so palette payloads are filtered by path; the busy
    // flag is cleared regardless (see onPaletteReady).
    Connections {
        target: drawer.app.paletteExtractor
        function onPaletteReady(path, colors, accents, seed) {
            // The busy flag is app-wide (one extraction at a time), so a
            // completion for a wallpaper the drawer no longer shows must
            // still clear it — otherwise the extract button stays
            // disabled on the new wallpaper.
            drawer.paletteBusy = false
            if (path === drawer.wallpaper) {
                drawer.paletteSwatches = colors
                drawer.paletteSelected = -1
            }
        }
        function onExtractionFailed(path, message) {
            drawer.paletteBusy = false
            if (path === drawer.wallpaper) {
                // qmllint disable unqualified
                const win = applicationWindow()
                // qmllint enable unqualified
                if (win)
                    win.showPassiveNotification(message, "long")
            }
        }
    }

    contentItem: Item {
        // Fixed header and footer: the body holds the page selector and
        // the current page's content.
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.gridUnit
            spacing: Kirigami.Units.gridUnit

            // Header: file name and close button.
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
                    icon.name: "documentinfo"
                    display: AbstractButton.IconOnly
                    Accessible.name: qsTr("Info")                    
                    ToolTip.text: qsTr("Info")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    checkable: true
                    checked: drawer.pageSelected === 0
                    onClicked: drawer.pageSelected = 0
                }

                ToolButton {
                    icon.name: "preferences-system-symbolic"
                    display: AbstractButton.IconOnly
                    Accessible.name: qsTr("Commands")                    
                    ToolTip.text: qsTr("Commands")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    checkable: true
                    checked: drawer.pageSelected === 1
                    onClicked: drawer.pageSelected = 1                   
                }                

                ToolButton {
                    icon.name: "dialog-close"
                    display: AbstractButton.IconOnly
                    Accessible.name: qsTr("Close")
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: drawer.close()
                }
            }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: Kirigami.Units.gridUnit

                Loader {
                    id: pageLoader
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    sourceComponent: drawer.pageSelected === 0
                        ? infoPageComponent
                        : commandsPageComponent
                }

                // Info page: media and the scrollable section.
                Component {
                    id: infoPageComponent

                    ColumnLayout {
                        anchors.fill: parent
                        spacing: Kirigami.Units.gridUnit

                        // Media area: the base layer is always the wallpaper (images)
                        // or its cached first frame (videos). Live playback is layered
                        // on top after a short delay, so the user sees the frame
                        // immediately instead of a blank area while the decoder starts.
                        Item {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 200
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

                            // The MediaPlayer lives inside the Loader so closing the
                            // drawer or switching wallpaper destroys it (not just
                            // pauses it), freeing the decoder and letting the grid
                            // cell's own video resume its loop.
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
                                            // -1 disables the audio track: wallpapers play muted.
                                            activeAudioTrack: -1
                                            videoOutput: videoSurface
                                        }
                                    }
                                }
                            }

                            // Arms the video 250 ms after the drawer opens.
                            Timer {
                                id: mediaStartTimer
                                interval: 250
                                running: drawer.isVideo && drawer.drawerOpen && !drawer.mediaArmed
                                onTriggered: drawer.mediaArmed = true
                            }
                        }

                        // Scrollable section: categories, tags and palette. The content
                        // column is pinned to availableWidth (the width minus the
                        // scrollbar) so it never scrolls horizontally.
                        ScrollView {
                            id: scrollView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            contentWidth: availableWidth
                            clip: true
                            ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

                            ColumnLayout {
                                id: content
                                width: scrollView.availableWidth
                                spacing: Kirigami.Units.gridUnit

                                // Categories and tags: category chips plus the refresh
                                // and add buttons, then the tag chips and the optional
                                // tag input. The "Add" menu only offers the categories
                                // the wallpaper does not have yet. Both chip rows wrap
                                // (Flow) so the buttons stay visible instead of
                                // overflowing the ScrollView's clip.
                                CollapsibleSection {
                                    Layout.fillWidth: true
                                    title: qsTr("Categories and tags")
                                    sectionContent: Component {
                                        ColumnLayout {
                                            width: parent.width
                                            spacing: Kirigami.Units.smallSpacing

                                            Flow {
                                                Layout.fillWidth: true
                                                spacing: Kirigami.Units.smallSpacing

                                                Repeater {
                                                    model: drawer.categories
                                                    delegate: Kirigami.Chip {
                                                        required property string modelData
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

                                                // Recalculates the automatic categories from the
                                                // wallpaper image and clears the manual override.
                                                ToolButton {
                                                    icon.name: "view-refresh"
                                                    Accessible.name: qsTr("Recalculate categories")
                                                    ToolTip.text: qsTr("Replace with the automatic categories")
                                                    ToolTip.visible: hovered
                                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                                    onClicked: drawer.app.wallpapersInfo.recalculateCategories(drawer.wallpaper)
                                                }

                                                ToolButton {
                                                    icon.name: "list-add"
                                                    Accessible.name: qsTr("Add category")
                                                    ToolTip.text: qsTr("Add category")
                                                    ToolTip.visible: hovered
                                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                                    onClicked: addMenu.open()
                                                }

                                                // Menu items are generated by the Instantiator
                                                // (a Menu does not support a Repeater directly) and
                                                // inserted/removed as the model changes.
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

                                            Flow {
                                                Layout.fillWidth: true
                                                spacing: Kirigami.Units.smallSpacing

                                                Repeater {
                                                    model: drawer.tags
                                                    delegate: Kirigami.Chip {
                                                        required property string modelData
                                                        width: implicitWidth
                                                        text: modelData
                                                        onRemoved: {
                                                            const t = drawer.tags.filter(t2 => t2 !== modelData)
                                                            drawer.tags = t
                                                            drawer.app.wallpapersInfo.setTags(drawer.wallpaper, t)
                                                        }
                                                    }
                                                }

                                                ToolButton {
                                                    Layout.alignment: Qt.AlignRight
                                                    icon.name: "list-add"
                                                    checkable: true
                                                    checked: drawer.tagInputVisible
                                                    display: AbstractButton.IconOnly
                                                    Accessible.name: qsTr("Show or hide the tag input")
                                                    ToolTip.text: qsTr("Show or hide the tag input")
                                                    ToolTip.visible: hovered
                                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                                    onClicked: drawer.tagInputVisible = checked
                                                }
                                            }

                                            TextField {
                                                id: tagField
                                                Layout.fillWidth: true
                                                visible: drawer.tagInputVisible
                                                onVisibleChanged: if (visible) forceActiveFocus()
                                                placeholderText: qsTr("Add tags, separated by commas...")
                                                onAccepted: {
                                                    // Comma-separated input adds several tags at once;
                                                    // empty parts and duplicates are ignored.
                                                    const parts = tagField.text.split(",").map(p => p.trim())
                                                    const list = drawer.tags.slice()
                                                    for (const t of parts) {
                                                        if (t !== "" && !list.includes(t))
                                                            list.push(t)
                                                    }
                                                    if (list.length !== drawer.tags.length) {
                                                        drawer.tags = list
                                                        drawer.app.wallpapersInfo.setTags(drawer.wallpaper, list)
                                                    }
                                                    tagField.text = ""
                                                }
                                            }
                                        }
                                    }
                                }

                                // Palette: backend controls, extracted swatches and the
                                // Material You scheme selector.
                                CollapsibleSection {
                                    Layout.fillWidth: true
                                    title: qsTr("Palette")
                                    sectionContent: Component {
                                        ColumnLayout {
                                            width: parent.width
                                            spacing: Kirigami.Units.smallSpacing

                                            // Palette controls: backend, extraction and mode.
                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Kirigami.Units.smallSpacing

                                                ComboBox {
                                                    id: backendCombo
                                                    Layout.preferredWidth: 160
                                                    model: [qsTr("Material You"), qsTr("ImageMagick")]
                                                    // Switching backend invalidates the current palette.
                                                    onActivated: {
                                                        drawer.paletteBackend = currentIndex === 1 ? "imagemagick" : "materialyou"
                                                        drawer.paletteSwatches = []
                                                        drawer.paletteSelected = -1
                                                    }
                                                }

                                                // Disabled while an extraction is running to avoid
                                                // overlapping requests for the same wallpaper.
                                                ToolButton {
                                                    Layout.alignment: Qt.AlignRight
                                                    enabled: !drawer.paletteBusy
                                                    icon.name: "media-playback-start"
                                                    Accessible.name: qsTr("Extract palette")
                                                    ToolTip.text: qsTr("Extract palette")
                                                    ToolTip.visible: hovered
                                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                                    onClicked: {
                                                        drawer.paletteBusy = true
                                                        drawer.app.paletteExtractor.extract(drawer.wallpaper, drawer.paletteBackend, drawer.paletteDark, drawer.paletteScheme)
                                                    }
                                                }

                                                // Dark/light only applies to Material You; the
                                                // ImageMagick backend ignores it.
                                                Switch {
                                                    id: darkSwitch
                                                    checked: drawer.paletteDark
                                                    enabled: drawer.paletteBackend === "materialyou"
                                                    onToggled: drawer.paletteDark = checked
                                                    Accessible.name: checked ? qsTr("Dark") : qsTr("Light")
                                                    ToolTip.text: checked ? qsTr("Dark") : qsTr("Light")
                                                    ToolTip.visible: hovered
                                                }
                                            }

                                            RowLayout {
                                                Layout.fillWidth: true
                                                spacing: Kirigami.Units.smallSpacing

                                                // Extracted swatches: left click selects, right click
                                                // copies the hex value to the clipboard.
                                                Grid {
                                                    id: paletteGrid
                                                    Layout.fillWidth: true
                                                    columns: 8
                                                    columnSpacing: Kirigami.Units.smallSpacing
                                                    rowSpacing: Kirigami.Units.smallSpacing

                                                    Repeater {
                                                        model: drawer.paletteSwatches
                                                        delegate: ColorDot {
                                                            required property string modelData
                                                            required property int index
                                                            color: modelData
                                                            selected: drawer.paletteSelected === index
                                                            onClicked: drawer.paletteSelected = index
                                                        }
                                                    }
                                                }

                                                // M3 scheme selector (Material You only). Switching
                                                // scheme re-requests the palette instead of clearing
                                                // it: the extractor keeps the nine schemes of an
                                                // extracted wallpaper in cache, so this is immediate
                                                // once the first extraction has run (a switch before
                                                // the first one simply extracts with the new scheme).
                                                ComboBox {
                                                    id: schemeCombo
                                                    visible: drawer.paletteBackend === "materialyou" && drawer.paletteSwatches.length > 0
                                                    model: ["TonalSpot", "Vibrant", "Expressive", "Content", "FruitSalad", "Rainbow", "Monochrome", "Neutral", "Fidelity"]
                                                    Accessible.name: qsTr("Material You scheme")
                                                    ToolTip.text: qsTr("Material You scheme")
                                                    ToolTip.visible: hovered
                                                    onActivated: {
                                                        drawer.paletteScheme = currentText
                                                        drawer.app.paletteExtractor.extract(drawer.wallpaper, drawer.paletteBackend, drawer.paletteDark, drawer.paletteScheme)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }

                // Commands page: placeholder for the custom-commands UI.
                Component {
                    id: commandsPageComponent

                    Item {
                    }
                }
            }

            // Footer: always visible, outside the scroll area.
            Kirigami.Separator { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                Button {
                    text: qsTr("Set as wallpaper")
                    icon.name: "preferences-desktop-wallpaper"
                    onClicked: drawer.app.applyWallpaper()
                }
            }
        }
    }
}