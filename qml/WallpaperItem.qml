import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami
import QtMultimedia

pragma ComponentBehavior: Bound

ItemDelegate {
    id: root
    required property AppController app
    required property string path
    required property string name
    required property url thumbnail
    required property string extension
    required property bool isVideo
    // Matugen source colors of this wallpaper (from the cache index);
    // shown as dots on the selected cell.
    required property var sourceColors
    required property GridView grid
    required property int index

    background: Item {}
    // The org.kde.desktop ItemDelegate uses asymmetric top/bottom padding (20/8)
    // tuned for vertical lists; in the GridView that shifts the content area down,
    // so the cell highlight looks uneven. Zero it out so the card is centered.
    topPadding: 2
    bottomPadding: 2
    leftPadding: 2
    rightPadding: 2
    // The grid is navigated through its view, never through cells: the cell
    // is not a tab stop (the desktop style already defaults ItemDelegate to
    // false, but being explicit keeps the chain identical in every style).
    activeFocusOnTab: false

    readonly property bool isSelected: root.path === root.app.selectedWallpaper
    // Whether the wallpapers page is currently visible; passed down by the page.
    property bool pageVisible: true
    // Whether the detail drawer is open; passed down by the page. While it
    // is, the drawer shows the video, so the cell's own live playback stays
    // off (no double decode of the same file).
    property bool detailOpen: false
    // The selected video cell plays live (muted, looping) instead of the
    // still. Media only exists while the cell is the active video, so at
    // most one MediaPlayer/VideoOutput exists at a time. Starting is
    // delayed (videoStartTimer) so the highlight transition is not lagged
    // by decoder/surface setup; stopping is immediate so switching tabs,
    // folders, selection or opening the drawer releases the GPU resources
    // right away.
    readonly property bool videoActive:
        root.isVideo && root.isSelected && root.pageVisible && !root.detailOpen
    // Animated GIF: QQuickImage only shows the first frame, so GIFs use
    // AnimatedImage (QMovie-based, plays the frames in place).
    readonly property bool isGif: root.extension.toLowerCase() === "gif"
    // Armed after the start delay; the Loader (whose active defaults to
    // true) is only loaded once armed, and it disarms itself immediately
    // whenever it stops.
    property bool videoArmed: false

    // Double-click on a cell: the page opens the detail drawer, which
    // shows the grid selection (this cell, since the click selected it).
    signal openDetail()

    // ItemDelegate (AbstractButton) swallows Space as its own click, so the
    // page's GridView-level handler never sees it while a cell has focus.
    // The attached handler runs first: Space = keyboard double-click.
    Keys.onSpacePressed: function(event) {
        event.accepted = true
        root.openDetail()
    }

    // Covers the highlight transition (GridView default ~250ms).
    Timer {
        id: videoStartTimer
        interval: 250
        running: root.videoActive
        onTriggered: root.videoArmed = true
    }

    hoverEnabled: true

    contentItem: Item {
        // Single click selects, double click opens the detail drawer.
        // Below all other children, so the apply button and color dots keep
        // their own click handling.
        MouseArea {
            id: cellMouse
            anchors.fill: parent
            // Clicking also keeps keyboard control with the grid, so the
            // arrow keys keep working right after a mouse selection.
            onClicked: {
                root.grid.currentIndex = root.index
                root.grid.forceActiveFocus()
            }
            onDoubleClicked: root.openDetail()
        }
        // Rounded clipped container like the old app: keeps the hover/selection zoom inside the cell.
        Rectangle {
            id: thumbContainer
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            color: "transparent"
            radius: Kirigami.Units.smallSpacing
            clip: true

            Image {
                id: thumbnailImage
                anchors.fill: parent
                source: root.thumbnail
                asynchronous: true
                smooth: true
                fillMode: Image.PreserveAspectCrop
                scale: root.hovered ? 1.14 : (root.isSelected ? 1.07 : 1.0)
                Behavior on scale {
                    SpringAnimation {
                        spring: 8.0
                        damping: 0.55
                        mass: 1.0
                    }
                }
                // Stays visible until the first video frame lands on top;
                // VideoOutput is transparent before the first frame.
                opacity: status === Image.Ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad } }
                visible: !root.isGif
            }
            // AnimatedImage cannot use QQuickImageProvider, so it loads the
            // real file URL instead of the thumbnail.
            AnimatedImage {
                id: gifImage
                anchors.fill: parent
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                smooth: true
                // AnimatedImage caches every frame in memory; cap the
                // stored frame width at 1280 so a big GIF does not exhaust
                // memory. The zero height lets Qt compute it from the GIF's
                // natural size, keeping the aspect ratio (a fixed square
                // would stretch the frames).
                sourceSize: Qt.size(1280, 0)
                source: root.isGif ? root.app.fileUrl(root.path) : ""
                visible: root.isGif
                // Like the video player, the animation pauses while the
                // detail panel shows the same file.
                playing: !root.detailOpen
                scale: root.hovered ? 1.14 : (root.isSelected ? 1.07 : 1.0)
                Behavior on scale {
                    SpringAnimation {
                        spring: 8.0
                        damping: 0.55
                        mass: 1.0
                    }
                }
                opacity: status === Image.Ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration } }
            }

            Loader {
                id: videoLoader
                anchors.fill: parent
                active: root.videoActive && root.videoArmed
                asynchronous: false
                onActiveChanged: if (!active)
                    root.videoArmed = false
                sourceComponent: Component {
                    Item {
                        VideoOutput {
                            id: videoSurface
                            anchors.fill: parent
                            fillMode: VideoOutput.PreserveAspectCrop
                        }
                        MediaPlayer {
                            id: mediaPlayer
                            source: root.app.fileUrl(root.path)
                            autoPlay: true
                            loops: MediaPlayer.Infinite
                            activeAudioTrack: -1
                            videoOutput: videoSurface
                        }
                    }
                }
            }
        }
        Kirigami.Icon {
            anchors.centerIn: parent
            source: "image-x-generic"
            color: Kirigami.Theme.disabledTextColor
            visible: !videoLoader.active
                && (root.thumbnail.toString() === ""
                    || thumbnailImage.status === Image.Error
                    || (root.isGif && gifImage.status === Image.Error))
        }
        ToolButton {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Kirigami.Units.smallSpacing
            icon.name: "preferences-desktop-wallpaper"
            display: AbstractButton.IconOnly
            visible: root.isSelected
            // Mouse affordance only: the keyboard applies with Enter on the
            // grid. Keeping it a tab stop would make a stray Tab+Enter apply
            // a wallpaper, and the chain would repeat it for every cell.
            activeFocusOnTab: false
            Accessible.name: qsTr("Set as Wallpaper")
            ToolTip.text: qsTr("Set as Wallpaper")
            ToolTip.visible: hovered
            onClicked: root.app.applyWallpaper()
        }
        Rectangle {
            id: badge
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.smallSpacing * 2
            width: badgeLabel.implicitWidth + Kirigami.Units.smallSpacing
            height: badgeLabel.implicitHeight
            radius: Kirigami.Units.smallSpacing
            color: Kirigami.Theme.alternateBackgroundColor
            border.color: Kirigami.Theme.highlightColor
            border.width: 0.5
            Label {
                id: badgeLabel
                anchors.centerIn: parent
                text: root.extension.toUpperCase()
                font: Kirigami.Theme.smallFont
                leftPadding: Kirigami.Units.smallSpacing
                rightPadding: Kirigami.Units.smallSpacing
                topPadding: Kirigami.Units.gridUnit / 4
                bottomPadding: Kirigami.Units.gridUnit / 4
            }
        }
        // The matugen source colors of the selected wallpaper, at the
        // bottom-left above the extension badge row.
        SourceColorDots {
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.smallSpacing * 2
            visible: root.isSelected
                && root.app.matugenDotsEnabled
                && root.sourceColors.length > 0
            app: root.app
            colors: root.sourceColors
            dotSize: Kirigami.Units.gridUnit * 0.8
        }
    }
}
