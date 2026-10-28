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
    // modal: true
    modal: false
    collapsible: false
    collapsed: false
    drawerOpen: false
    handleVisible: false
    dragMargin: 0
    closePolicy: Popup.CloseOnEscape
    // qmllint disable unqualified
    preferredSize: applicationWindow() ? Math.round(applicationWindow().width * 0.6) : 400
    // qmllint enable unqualified
    interactiveResizeEnabled: true

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

    // AI tag results for the wallpaper the drawer shows; the tag chips
    // themselves resync through entryChanged.
    function notify(message) {
        // qmllint disable unqualified
        const win = applicationWindow()
        // qmllint enable unqualified
        if (win) {
            win.showPassiveNotification(message, "long")
        }
    }

    Connections {
        target: drawer.app.aiTagger
        function onWallpaperTagged(path, ok, message, tags) {
            if (path === drawer.wallpaper) {
                drawer.notify(ok
                    ? qsTr("Tags updated (%1)").arg(tags.length)
                    : qsTr("AI tagging failed: %1").arg(message))
            }
        }
        function onError(message) {
            drawer.notify(message)
        }
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


            // Media area: the base layer is always the wallpaper (images)
            // or its cached first frame (videos). Live playback is layered
            // on top after a short delay, so the user sees the frame
            // immediately instead of a blank area while the decoder starts.
            // The matugen source colors overlay its bottom-left corner.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 200
                clip: true

                Rectangle {
                id: mediaBorder
                    anchors.fill: parent
                    anchors.margins: 0
                    color: "transparent"
                    border.color: Qt.alpha(Kirigami.Theme.textColor, 0.30)
                    border.width: 2
                    radius: Kirigami.Units.smallSpacing
                    z: 10
                }

                Image {
                    id: mediaImage
                    anchors.fill: parent
                    anchors.margins: 2
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
                    anchors.margins: 2
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
                // The matugen source colors, overlaid at the bottom-left
                // like the grid's selected cell: the selected dot feeds the
                // %scolor%/%sc% placeholders of the custom commands.
                Rectangle {
                    id: sourceDotsContainer
                    anchors.left: parent.left
                    anchors.bottom: parent.bottom
                    anchors.margins: Kirigami.Units.smallSpacing * 2
                    visible: drawer.app.matugenDotsEnabled
                        && drawer.app.wallpapersInfo.sourceColorsFor(drawer.wallpaper).length > 0
                    color: "transparent"
                    width: sourceDots.implicitWidth + Kirigami.Units.smallSpacing * 2
                    height: sourceDots.implicitHeight + Kirigami.Units.smallSpacing * 2

                    RowLayout {
                        id: sourceDots
                        anchors.centerIn: parent
                        spacing: 2

                        Repeater {
                            model: drawer.app.wallpapersInfo.sourceColorsFor(drawer.wallpaper)
                            delegate: ColorDot {
                                required property string modelData
                                required property int index
                                id: sourceDot
                                dotSize: Kirigami.Units.gridUnit * 0.95
                                color: sourceDot.modelData
                                selected: drawer.app.selectedSourceColorIndex === sourceDot.index
                                onClicked: drawer.app.selectedSourceColorIndex = sourceDot.index
                            }
                        }
                    }
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

                        // Scrollable section: categories, tags and palette. The content
                        // column is pinned to availableWidth (the width minus the
                        // scrollbar) so it never scrolls horizontally.
                        ScrollView {
                            id: scrollView
                            Layout.fillWidth: true
                            Layout.fillHeight: true
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

                                                // Replaces the tags with AI suggestions from the configured
                                                // OpenAI-compatible endpoint (Settings → Wallpapers).
                                                ToolButton {
                                                    icon.name: "tag-new"
                                                    enabled: !drawer.app.aiTagger.tagging
                                                    Accessible.name: qsTr("Generate tags with AI")
                                                    ToolTip.text: qsTr("Add AI-suggested tags to the existing ones")
                                                    ToolTip.visible: hovered
                                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                                    onClicked: {
                                                        if (drawer.app.aiTagger.endpoint === "" || drawer.app.aiTagger.model === "") {
                                                            drawer.notify(qsTr("Set the AI endpoint and model in Settings → Wallpapers first."))
                                                        } else {
                                                            drawer.app.aiTagger.tagWallpaper(drawer.wallpaper)
                                                        }
                                                    }
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

                // Commands page: the %mode%/%ccolor% placeholder controls
                // (dark/light switch and custom color) and the custom
                // commands list, executed last on every wallpaper apply.
                // The matugen source colors (the %scolor%/%sc% placeholders,
                // first dot selected by default) are overlaid on the media.
                Component {
                    id: commandsPageComponent

                    ScrollView {
                        id: scrollView2
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff
                        ColumnLayout {
                            width: scrollView2.availableWidth
                            spacing: Kirigami.Units.gridUnit

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: Kirigami.Units.smallSpacing

                                Kirigami.Icon {
                                    source: "lighttable-symbolic"
                                    Layout.preferredWidth: Kirigami.Units.gridUnit
                                    Layout.preferredHeight: Kirigami.Units.gridUnit
                                    color: Kirigami.Theme.highlightColor
                                }

                                Switch {
                                    Layout.fillHeight: true
                                    checked: drawer.app.themeMode === "dark"
                                    Accessible.name: qsTr("Dark mode")
                                    ToolTip.text: qsTr("%mode% placeholder: dark or light")
                                    ToolTip.visible: hovered
                                    ToolTip.delay: Kirigami.Units.toolTipDelay
                                    onToggled: drawer.app.themeMode = checked ? "dark" : "light"
                                }

                                Label {
                                    text: drawer.app.themeMode === "dark" ? qsTr("Dark") : qsTr("Light")
                                    color: Kirigami.Theme.disabledTextColor
                                    font: Kirigami.Theme.smallFont
                                }

                                Item { Layout.fillWidth: true }

                                Label {
                                    text: qsTr("Custom color:")
                                    color: Kirigami.Theme.disabledTextColor
                                    font: Kirigami.Theme.smallFont
                                }

                                ColorDot {
                                    Layout.alignment: Qt.AlignVCenter
                                    //dotSize: Kirigami.Units.gridUnit
                                    color: drawer.app.customColor ? drawer.app.customColor : Kirigami.Theme.highlightColor
                                    enablePicker: true
                                    onAccepted: (hex) => drawer.app.customColor = hex
                                }
                            }

                            CollapsibleSection {
                                Layout.fillWidth: true
                                title: qsTr("Custom commands")
                                sectionContent: Component {
                                    ColumnLayout {
                                        width: parent.width
                                        spacing: Kirigami.Units.smallSpacing

                                        ListModel { id: commandModel }

                                        Component.onCompleted: {
                                            commandModel.clear()
                                            for (const c of drawer.app.customCommands) {
                                                commandModel.append({ command: c.command,
                                                                        isEnabled: c.enabled })
                                            }
                                        }

                                        ListView {
                                            id: commandList
                                            Layout.fillWidth: true
                                            Layout.preferredHeight: contentHeight
                                            interactive: false
                                            spacing: Kirigami.Units.smallSpacing
                                            model: commandModel
                                            moveDisplaced: Transition {
                                                YAnimator {
                                                    duration: Kirigami.Units.longDuration
                                                    easing.type: Easing.InOutQuad
                                                }
                                            }

                                            delegate: Item {
                                                id: commandDelegate
                                                required property int index
                                                required property string command
                                                required property bool isEnabled

                                                width: commandList.width
                                                height: commandRow.implicitHeight

                                                ItemDelegate {
                                                    id: commandRow
                                                    width: parent.width
                                                    contentItem: RowLayout {
                                                        spacing: Kirigami.Units.smallSpacing

                                                        Kirigami.ListItemDragHandle {
                                                            Layout.alignment: Qt.AlignVCenter
                                                            enabled: commandList.model.count > 1
                                                            opacity: enabled ? 1 : 0.3
                                                            listItem: commandRow
                                                            listView: commandList
                                                            onMoveRequested: (oldIndex, newIndex) => {
                                                                commandModel.move(oldIndex, newIndex, 1)
                                                                drawer.app.moveCustomCommand(oldIndex, newIndex)
                                                            }
                                                        }

                                                        Label {
                                                            Layout.fillHeight: true
                                                            text: String(commandDelegate.index + 1)
                                                            horizontalAlignment: Text.AlignHCenter
                                                            verticalAlignment: Text.AlignVCenter
                                                            font.bold: true
                                                            color: commandDelegate.isEnabled ? Kirigami.Theme.highlightColor : Kirigami.Theme.disabledTextColor
                                                        }

                                                        Switch {
                                                            Layout.fillHeight: true
                                                            checked: commandDelegate.isEnabled
                                                            Accessible.name: checked ? qsTr("Enabled") : qsTr("Disabled")
                                                            onToggled: drawer.app.setCustomCommandEnabled(commandDelegate.index, checked)
                                                        }

                                                        TextField {
                                                            id: commandField
                                                            Layout.fillWidth: true
                                                            text: commandDelegate.command
                                                            placeholderText: qsTr("e.g.: notify-send 'Wallpaper changed to %path%'")
                                                            onEditingFinished: drawer.app.updateCustomCommand(commandDelegate.index, commandField.text)
                                                        }

                                                        ToolButton {
                                                            icon.name: "edit-clear"
                                                            Accessible.name: qsTr("Remove command")
                                                            ToolTip.text: qsTr("Remove command")
                                                            ToolTip.visible: hovered
                                                            ToolTip.delay: Kirigami.Units.toolTipDelay
                                                            onClicked: {
                                                                const i = commandDelegate.index
                                                                commandModel.remove(i)
                                                                drawer.app.removeCustomCommand(i)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }

                                        ToolButton {
                                            text: qsTr("Add command")
                                            icon.name: "list-add"
                                            ToolTip.text: qsTr("Placeholders:<br>" +"<b>%path%</b> wallpaper path<br>" +"<b>%image%</b> image (or video frame) path<br>" +"<b>%scolor%</b> selected source color<br>" +"<b>%sc%</b> matugen source color index<br>" +"<b>%mode%</b> dark/light mode<br>" +"<b>%ccolor%</b> custom color.")
                                            ToolTip.visible: hovered
                                            ToolTip.delay: Kirigami.Units.toolTipDelay
                                            onClicked: {
                                                commandModel.append({ command: "", isEnabled: true })
                                                drawer.app.addCustomCommand()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Footer: always visible, outside the scroll area.
            Kirigami.Separator { Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                ToolButton {
                    text: qsTr("Set as wallpaper")
                    icon.name: "preferences-desktop-wallpaper"
                    onClicked: drawer.app.applyWallpaper()
                }
            }
        }
    }
}