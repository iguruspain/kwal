import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtMultimedia
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// Inline panel beside the grid (not an overlay); the page animates its width like the sidebar.
Rectangle {
    id: root

    required property AppController app
    required property string wallpaper
    required property bool isVideo
    required property var categorySwatches
    // While open, the grid cell's own player stays off (live playback moves here).
    property bool open: false

    signal closeRequested()

    // Reassigned, never mutated in place, so bindings re-evaluate; writes go through WallpapersInfo.
    property var categories: []
    property var tags: []
    // Mirrored (entryChanged re-fetch), not bound: *For() methods are not notifiable properties.
    property var sourceColors: []
    property bool tagInputVisible: false

    // QImageReader::imageCount() probe cached in WallpapersInfo (extension fallback until probed): keeps static WebP/GIF off the QMovie path.
    property bool isAnimated: false

    property string paletteBackend: "materialyou"
    property bool paletteDark: true
    property string paletteScheme: "TonalSpot"
    property var paletteSwatches: []
    property int paletteSelected: -1
    property bool paletteBusy: false
    // Timer-set after opening so the first frame shows before the decoder starts; reset on Loader unload.
    property bool mediaArmed: false
    property real mediaFrameWidth: 512

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

    onOpenChanged: if (open) {
        pageSelected = 0
        mediaWidthSettle.restart()
    }

    function syncWithInfo() {
        categories = app.wallpapersInfo.colorCategoriesFor(root.wallpaper)
        tags = app.wallpapersInfo.tagsFor(root.wallpaper)
        sourceColors = app.wallpapersInfo.sourceColorsFor(root.wallpaper)
        isAnimated = app.wallpapersInfo.isAnimatedFor(root.wallpaper)
        paletteSwatches = []
        paletteSelected = -1
        // paletteBusy is app-wide: a discarded completion must not leave it stuck (see onPaletteReady).
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

    // split() returns a plain JS array (no last()).
    function fileNameOf(path) {
        const parts = path.split("/")
        return parts[parts.length - 1]
    }

    Connections {
        target: root.app.wallpapersInfo
        function onEntryChanged(path) {
            if (path === root.wallpaper) {
                root.syncWithInfo()
            }
        }
    }

    // Extractions are async: results for other wallpapers arrive late — filter payloads by path, clear the busy flag regardless.
    Connections {
        target: root.app.paletteExtractor
        function onPaletteReady(path, colors, accents, seed) {
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

        // Base layer = wallpaper or cached first frame; live playback layers on top after a delay (frame visible while the decoder starts).
        Item {
            id: mediaArea
            // 16:9, height capped at 40% of panel height; width derives from height so the cap shrinks it proportionally.
            readonly property real mediaHeight: Math.min(
                (rootContent.width - 2 * Kirigami.Units.gridUnit) * 9 / 16,
                root.height * 0.4)
            onWidthChanged: mediaWidthSettle.restart()
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
                // sourceSize caps the decode; without it a 4K source decodes at native size.
                sourceSize: Qt.size(1280, 0)
                visible: !root.isAnimated
                source: root.isVideo
                    ? (root.app.wallpapersInfo.firstFrameFor(root.wallpaper) !== ""
                        ? root.app.fileUrl(root.app.wallpapersInfo.firstFrameFor(root.wallpaper))
                        : "")
                    : root.app.fileUrl(root.wallpaper)
            }

            // setScaledSize is a decode target, not a cap; every change reloads all frames — feed it only the settled width.
            Timer {
                id: mediaWidthSettle
                interval: 400
                onTriggered: if (mediaArea.width > 0)
                    root.mediaFrameWidth = Math.min(Math.ceil(mediaArea.width * root.Screen.devicePixelRatio), 1024)
            }

            // QQuickImage only shows the first frame; AnimatedImage decodes the GIF/WebP frames.
            AnimatedImage {
                id: mediaAnimated
                anchors.fill: parent
                anchors.margins: 2
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                smooth: true
                sourceSize: Qt.size(Math.round(root.mediaFrameWidth), 0)
                source: root.isAnimated ? root.app.fileUrl(root.wallpaper) : ""
                visible: root.isAnimated
                playing: root.isAnimated && root.open
            }

            // Loader destroys the MediaPlayer on close (not just pauses), freeing the decoder so the grid cell's loop resumes.
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
                            activeAudioTrack: -1
                            videoOutput: videoSurface
                        }
                    }
                }
            }

            Timer {
                id: mediaStartTimer
                interval: 250
                running: root.isVideo && root.open && !root.mediaArmed
                onTriggered: root.mediaArmed = true
            }

            // The selected dot feeds the %scolor%/%sc% placeholders of the custom commands.
            SourceColorDots {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Kirigami.Units.smallSpacing * 2
                visible: root.app.matugenDotsEnabled
                    && root.sourceColors.length > 0
                app: root.app
                colors: root.sourceColors
                dotSize: Kirigami.Units.gridUnit * 0.95
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

            Component {
                id: infoPageComponent

                ColumnLayout {
                    anchors.fill: parent
                    spacing: Kirigami.Units.gridUnit

                    // Content column pinned to availableWidth so it never scrolls horizontally.
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

                            // The "Add" menu only offers categories the wallpaper lacks; Flow keeps buttons visible inside the clip.
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

                                            // A Menu does not support a Repeater; items come from the Instantiator.
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

                            CollapsibleSection {
                                Layout.fillWidth: true
                                title: qsTr("Palette")
                                sectionContent: Component {
                                    ColumnLayout {
                                        width: parent.width
                                        spacing: Kirigami.Units.smallSpacing

                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: Kirigami.Units.smallSpacing

                                            ComboBox {
                                                id: backendCombo
                                                Layout.preferredWidth: 160
                                                model: [qsTr("Material You"), qsTr("ImageMagick")]
                                                onActivated: {
                                                    root.paletteBackend = currentIndex === 1 ? "imagemagick" : "materialyou"
                                                    root.paletteSwatches = []
                                                    root.paletteSelected = -1
                                                }
                                            }

                                            // One extraction at a time.
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

                                            // Dark/light applies to Material You only; ImageMagick ignores it.
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
                                        // Scheme switch re-extracts rather than clears: the extractor caches all nine schemes once extracted.
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

            // Commands page: %mode%/%ccolor% controls; %scolor%/%sc% dots live on the media, first selected by default.
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
