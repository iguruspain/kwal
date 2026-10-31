import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// Wallpaper detail panel (no overlay): lives in the wallpapers page's
// RowLayout, right of the grid, and pushes it aside — the page animates
// the panel's width and opacity exactly like the Folders sidebar. Opened
// by double-clicking a cell; closed via the X button or the Escape key.
Rectangle {
    id: root

    required property AppController app
    // Absolute path of the wallpaper shown in the panel. Changing it
    // reloads categories/tags and resets the palette and video state.
    required property string wallpaper
    required property bool isVideo
    // The 15 color categories (name/hex/label) shared with the search chips.
    required property var categorySwatches
    // Whether the panel is open; drives live video playback (the grid
    // cell keeps its own player off while it is open).
    property bool open: false

    signal closeRequested()

    // Local copies of the wallpaper's categories/tags/source colors.
    // They are reassigned (never mutated in place) so bindings
    // re-evaluate; the editable ones are written back through
    // WallpapersInfo.
    property var categories: []
    property var tags: []
    // Mirrored so the matugen dots appear as soon as the extraction job
    // finishes (entryChanged), not only when the panel happens to open
    // after it — the common case for videos (first-frame extraction).
    property var sourceColors: []
    // Whether the tag input field is shown; toggled by the "+" button.
    property bool tagInputVisible: false

    // Animated GIF: QQuickImage only shows the first frame, so the media
    // area uses AnimatedImage (QMovie-based) for them.
    readonly property bool isGif: {
        const dot = root.wallpaper.lastIndexOf(".")
        return dot > 0
            && root.wallpaper.substring(dot + 1).toLowerCase() === "gif"
    }

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

    color: Qt.alpha(Kirigami.Theme.backgroundColor, app.transparencyLevel / 100.0)
    radius: Kirigami.Units.largeSpacing
    border.color: Kirigami.Theme.highlightColor
    border.width: 1.5

    onWallpaperChanged: {
        syncWithInfo()
        mediaArmed = false
        pageSelected = 0
    }

    // The panel always opens on the Info page.
    onOpenChanged: if (open)
        pageSelected = 0

    // Reloads the manual categories/tags from the backend and clears any
    // palette computed for the previous wallpaper.
    function syncWithInfo() {
        categories = app.wallpapersInfo.colorCategoriesFor(root.wallpaper)
        tags = app.wallpapersInfo.tagsFor(root.wallpaper)
        sourceColors = app.wallpapersInfo.sourceColorsFor(root.wallpaper)
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

    // AI tag results for the wallpaper the panel shows; the tag chips
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
        target: root.app.aiTagger
        function onWallpaperTagged(path, ok, message, tags) {
            if (path === root.wallpaper) {
                root.notify(ok
                    ? qsTr("Tags updated (%1)").arg(tags.length)
                    : qsTr("AI tagging failed: %1").arg(message))
            }
        }
        function onError(message) {
            root.notify(message)
        }
    }

    // QML split() returns a plain JS array (no last()), so index the end.
    function fileNameOf(path) {
        const parts = path.split("/")
        return parts[parts.length - 1]
    }

    // Keeps the panel in sync when the entry is modified elsewhere
    // (e.g. a rescan or an edit from the grid).
    Connections {
        target: root.app.wallpapersInfo
        function onEntryChanged(path) {
            if (path === root.wallpaper) {
                root.syncWithInfo()
            }
        }
    }

    // Extraction is asynchronous and results for other wallpapers can
    // arrive late, so palette payloads are filtered by path; the busy
    // flag is cleared regardless (see onPaletteReady).
    Connections {
        target: root.app.paletteExtractor
        function onPaletteReady(path, colors, accents, seed) {
            // The busy flag is app-wide (one extraction at a time), so a
            // completion for a wallpaper the panel no longer shows must
            // still clear it — otherwise the extract button stays
            // disabled on the new wallpaper.
            root.paletteBusy = false
            if (path === root.wallpaper) {
                root.paletteSwatches = colors
                root.paletteSelected = -1
            }
        }
        function onExtractionFailed(path, message) {
            root.paletteBusy = false
            if (path === root.wallpaper) {
                root.notify(message)
            }
        }
    }

    ColumnLayout {
        id: rootContent
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
                text: root.fileNameOf(root.wallpaper)
            }

            ToolButton {
                icon.name: "documentinfo"
                display: AbstractButton.IconOnly
                Accessible.name: qsTr("Info")
                ToolTip.text: qsTr("Info")
                ToolTip.visible: hovered
                ToolTip.delay: Kirigami.Units.toolTipDelay
                checkable: true
                checked: root.pageSelected === 0
                onClicked: root.pageSelected = 0
            }

            ToolButton {
                icon.name: "preferences-system-symbolic"
                display: AbstractButton.IconOnly
                Accessible.name: qsTr("Commands")
                ToolTip.text: qsTr("Commands")
                ToolTip.visible: hovered
                ToolTip.delay: Kirigami.Units.toolTipDelay
                checkable: true
                checked: root.pageSelected === 1
                onClicked: root.pageSelected = 1
            }

            ToolButton {
                icon.name: "dialog-close"
                display: AbstractButton.IconOnly
                Accessible.name: qsTr("Close")
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: root.closeRequested()
            }
        }

        // Media area: the base layer is always the wallpaper (images)
        // or its cached first frame (videos). Live playback is layered
        // on top after a short delay, so the user sees the frame
        // immediately instead of a blank area while the decoder starts.
        // The matugen source colors overlay its bottom-left corner.
        Item {
            // The media area stays 16:9: the height starts from the full
            // content width and is capped at 40% of the panel height (so
            // the chips and palette below stay visible without scrolling);
            // the width is then derived from the height, so when the cap
            // kicks in the area shrinks proportionally instead of keeping
            // the full width.
            readonly property real mediaHeight: Math.min(
                (rootContent.width - 2 * Kirigami.Units.gridUnit) * 9 / 16,
                root.height * 0.4)
            Layout.preferredWidth: mediaHeight * 16 / 9
            Layout.preferredHeight: mediaHeight
            Layout.alignment: Qt.AlignHCenter
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
                visible: !root.isGif
                source: root.isVideo
                    ? (root.app.wallpapersInfo.firstFrameFor(root.wallpaper) !== ""
                        ? root.app.fileUrl(root.app.wallpapersInfo.firstFrameFor(root.wallpaper))
                        : "")
                    : root.app.fileUrl(root.wallpaper)
            }

            // Plays the GIF frames in place while the panel is open
            // (QQuickImage above only shows the first frame).
            AnimatedImage {
                id: mediaGif
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                smooth: true
                // AnimatedImage caches every frame in memory; cap the
                // stored frame width at 1280 so a big GIF does not exhaust
                // memory. The zero height lets Qt compute it from the GIF's
                // natural size, keeping the aspect ratio (a fixed square
                // would stretch the frames).
                sourceSize: Qt.size(1280, 0)
                source: root.isGif ? root.app.fileUrl(root.wallpaper) : ""
                visible: root.isGif
                playing: root.isGif && root.open
            }

            // The MediaPlayer lives inside the Loader so closing the
            // panel or switching wallpaper destroys it (not just
            // pauses it), freeing the decoder and letting the grid
            // cell's own video resume its loop.
            Loader {
                id: videoLoader
                anchors.fill: parent
                anchors.margins: 2
                active: root.isVideo && root.open && root.mediaArmed
                onActiveChanged: if (!active) root.mediaArmed = false
                sourceComponent: Component {
                    Item {
                        VideoOutput {
                            id: videoSurface
                            anchors.fill: parent
                            fillMode: VideoOutput.PreserveAspectCrop
                        }
                        MediaPlayer {
                            source: root.app.fileUrl(root.wallpaper)
                            autoPlay: true
                            loops: MediaPlayer.Infinite
                            // -1 disables the audio track: wallpapers play muted.
                            activeAudioTrack: -1
                            videoOutput: videoSurface
                        }
                    }
                }
            }

            // Arms the video 250 ms after the panel opens.
            Timer {
                id: mediaStartTimer
                interval: 250
                running: root.isVideo && root.open && !root.mediaArmed
                onTriggered: root.mediaArmed = true
            }

            // The matugen source colors, overlaid at the bottom-left
            // like the grid's selected cell: the selected dot feeds the
            // %scolor%/%sc% placeholders of the custom commands.
            Rectangle {
                id: sourceDotsContainer
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Kirigami.Units.smallSpacing * 2
                visible: root.app.matugenDotsEnabled
                    && root.sourceColors.length > 0
                color: "transparent"
                width: sourceDots.implicitWidth + Kirigami.Units.smallSpacing * 2
                height: sourceDots.implicitHeight + Kirigami.Units.smallSpacing * 2

                RowLayout {
                    id: sourceDots
                    anchors.centerIn: parent
                    spacing: 2

                    Repeater {
                        model: root.sourceColors
                        delegate: ColorDot {
                            required property string modelData
                            required property int index
                            id: sourceDot
                            dotSize: Kirigami.Units.gridUnit * 0.95
                            color: sourceDot.modelData
                            selected: root.app.selectedSourceColorIndex === sourceDot.index
                            onClicked: root.app.selectedSourceColorIndex = sourceDot.index
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
                sourceComponent: root.pageSelected === 0
                    ? infoPageComponent
                    : commandsPageComponent
            }

            // Info page: the scrollable section (media stays fixed above).
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
                                                model: root.categories
                                                delegate: Kirigami.Chip {
                                                    required property string modelData
                                                    width: implicitWidth
                                                    text: modelData
                                                    icon.name: "tag"
                                                    icon.color: root.hexFor(modelData)
                                                    onRemoved: {
                                                        const c = root.categories.filter(c2 => c2 !== modelData)
                                                        root.categories = c
                                                        root.app.wallpapersInfo.setCategories(root.wallpaper, c)
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
                                                    model: root.categorySwatches.filter(s => !root.categories.includes(s.name))
                                                    delegate: MenuItem {
                                                        required property var modelData
                                                        text: modelData.label
                                                        icon.name: "tag"
                                                        icon.color: modelData.hex
                                                        onTriggered: {
                                                            const list = root.categories.slice()
                                                            list.push(modelData.name)
                                                            root.categories = list
                                                            root.app.wallpapersInfo.setCategories(root.wallpaper, list)
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
                                                onClicked: root.app.wallpapersInfo.recalculateCategories(root.wallpaper)
                                            }

                                        }

                                        Kirigami.Separator { Layout.fillWidth: true }

                                        Flow {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            Repeater {
                                                model: root.tags
                                                delegate: Kirigami.Chip {
                                                    required property string modelData
                                                    width: implicitWidth
                                                    text: modelData
                                                    onRemoved: {
                                                        const t = root.tags.filter(t2 => t2 !== modelData)
                                                        root.tags = t
                                                        root.app.wallpapersInfo.setTags(root.wallpaper, t)
                                                    }
                                                }
                                            }

                                            ToolButton {
                                                Layout.alignment: Qt.AlignRight
                                                icon.name: "list-add"
                                                checkable: true
                                                checked: root.tagInputVisible
                                                display: AbstractButton.IconOnly
                                                Accessible.name: qsTr("Show or hide the tag input")
                                                ToolTip.text: qsTr("Show or hide the tag input")
                                                ToolTip.visible: hovered
                                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                                onClicked: root.tagInputVisible = checked
                                            }

                                            // Replaces the tags with AI suggestions from the configured
                                            // OpenAI-compatible endpoint (Settings → Wallpapers).
                                            ToolButton {
                                                icon.name: "tag-new"
                                                enabled: !root.app.aiTagger.tagging
                                                Accessible.name: qsTr("Generate tags with AI")
                                                ToolTip.text: qsTr("Add AI-suggested tags to the existing ones")
                                                ToolTip.visible: hovered
                                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                                onClicked: {
                                                    if (root.app.aiTagger.endpoint === "" || root.app.aiTagger.model === "") {
                                                        root.notify(qsTr("Set the AI endpoint and model in Settings → Wallpapers first."))
                                                    } else {
                                                        root.app.aiTagger.tagWallpaper(root.wallpaper)
                                                    }
                                                }
                                            }
                                        }

                                        TextField {
                                            id: tagField
                                            Layout.fillWidth: true
                                            visible: root.tagInputVisible
                                            onVisibleChanged: if (visible) forceActiveFocus()
                                            placeholderText: qsTr("Add tags, separated by commas...")
                                            onAccepted: {
                                                // Comma-separated input adds several tags at once;
                                                // empty parts and duplicates are ignored.
                                                const parts = tagField.text.split(",").map(p => p.trim())
                                                const list = root.tags.slice()
                                                for (const t of parts) {
                                                    if (t !== "" && !list.includes(t))
                                                        list.push(t)
                                                }
                                                if (list.length !== root.tags.length) {
                                                    root.tags = list
                                                    root.app.wallpapersInfo.setTags(root.wallpaper, list)
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
                                                    root.paletteBackend = currentIndex === 1 ? "imagemagick" : "materialyou"
                                                    root.paletteSwatches = []
                                                    root.paletteSelected = -1
                                                }
                                            }

                                            // Disabled while an extraction is running to avoid
                                            // overlapping requests for the same wallpaper.
                                            ToolButton {
                                                Layout.alignment: Qt.AlignRight
                                                enabled: !root.paletteBusy
                                                icon.name: "media-playback-start"
                                                Accessible.name: qsTr("Extract palette")
                                                ToolTip.text: qsTr("Extract palette")
                                                ToolTip.visible: hovered
                                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                                onClicked: {
                                                    root.paletteBusy = true
                                                    root.app.paletteExtractor.extract(root.wallpaper, root.paletteBackend, root.paletteDark, root.paletteScheme)
                                                }
                                            }

                                            // Dark/light only applies to Material You; the
                                            // ImageMagick backend ignores it.
                                            Switch {
                                                id: darkSwitch
                                                checked: root.paletteDark
                                                enabled: root.paletteBackend === "materialyou"
                                                onToggled: root.paletteDark = checked
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
                                                    model: root.paletteSwatches
                                                    delegate: ColorDot {
                                                        required property string modelData
                                                        required property int index
                                                        color: modelData
                                                        selected: root.paletteSelected === index
                                                        onClicked: root.paletteSelected = index
                                                    }
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
                                            visible: root.paletteBackend === "materialyou" && root.paletteSwatches.length > 0
                                            model: ["TonalSpot", "Vibrant", "Expressive", "Content", "FruitSalad", "Rainbow", "Monochrome", "Neutral", "Fidelity"]
                                            Accessible.name: qsTr("Material You scheme")
                                            ToolTip.text: qsTr("Material You scheme")
                                            ToolTip.visible: hovered
                                            onActivated: {
                                                root.paletteScheme = currentText
                                                root.app.paletteExtractor.extract(root.wallpaper, root.paletteBackend, root.paletteDark, root.paletteScheme)
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
                                checked: root.app.themeMode === "dark"
                                Accessible.name: qsTr("Dark mode")
                                ToolTip.text: qsTr("%mode% placeholder: dark or light")
                                ToolTip.visible: hovered
                                ToolTip.delay: Kirigami.Units.toolTipDelay
                                onToggled: root.app.themeMode = checked ? "dark" : "light"
                            }

                            Label {
                                text: root.app.themeMode === "dark" ? qsTr("Dark") : qsTr("Light")
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
                                color: root.app.customColor ? root.app.customColor : Kirigami.Theme.highlightColor
                                dotSize: Kirigami.Units.gridUnit * 0.95
                                enablePicker: true
                                onAccepted: (hex) => root.app.customColor = hex
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
                                        for (const c of root.app.customCommands) {
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
                                                            root.app.moveCustomCommand(oldIndex, newIndex)
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
                                                        onToggled: root.app.setCustomCommandEnabled(commandDelegate.index, checked)
                                                    }

                                                    TextField {
                                                        id: commandField
                                                        Layout.fillWidth: true
                                                        text: commandDelegate.command
                                                        placeholderText: qsTr("e.g.: notify-send 'Wallpaper changed to %path%'")
                                                        onEditingFinished: root.app.updateCustomCommand(commandDelegate.index, commandField.text)
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
                                                            root.app.removeCustomCommand(i)
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
                                            root.app.addCustomCommand()
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
                onClicked: root.app.applyWallpaper()
            }
        }
    }
}
