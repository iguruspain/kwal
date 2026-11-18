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
    // Probed QImageReader::imageCount (extension fallback while unprobed): static GIF/WebP stays on the plain thumbnail path.
    required property bool isAnimated
    required property var sourceColors
    required property GridView grid
    required property int index

    background: Item {}
    // org.kde.desktop ItemDelegate pads 20/8 top/bottom (list-tuned); zero it or the card sits off-center in the grid.
    topPadding: 2
    bottomPadding: 2
    leftPadding: 2
    rightPadding: 2
    // Never a tab stop: the view is the grid's focus entry (explicit, so every style matches).
    activeFocusOnTab: false

    readonly property bool isSelected: root.path === root.app.selectedWallpaper
    property bool pageVisible: true
    // While the drawer is open it owns video playback: cell playback off (no double decode).
    property bool detailOpen: false
    // At most one MediaPlayer exists while this cell is the active video; start delayed to keep the highlight transition smooth, stop immediate to release the decoder.
    readonly property bool videoActive:
        root.isVideo && root.isSelected && root.pageVisible && !root.detailOpen
    // QMovie decodes on the GUI thread and restarts when a reused delegate rebinds the source — only the selected, visible cell plays; others show their static first-frame thumbnail.
    readonly property bool animatedActive:
        root.isAnimated && root.isSelected && root.pageVisible && !root.detailOpen
    // QMovie's scaled size is a target, not a cap: cellWidth x dpr covers HiDPI, topped at 768 so a grid cell's frame cache cannot balloon.
    readonly property int animatedFrameWidth:
        Math.min(Math.ceil(root.grid.cellWidth * root.Screen.devicePixelRatio), 768)
    // Armed after the start delay; the Loader unarms whenever it deactivates.
    property bool animatedArmed: false
    // Same for the video Loader: it only loads once armed and disarms on stop.
    property bool videoArmed: false

    signal openDetail()

    // ItemDelegate (AbstractButton) swallows Space as its own click, so the grid-level handler never sees it; the attached handler runs first: Space = keyboard double-click.
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

    // Same delay so a GIF/WebP decode cannot race the highlight transition.
    Timer {
        id: animatedStartTimer
        interval: 250
        running: root.animatedActive
        onTriggered: root.animatedArmed = true
    }

    hoverEnabled: true

    contentItem: Item {
        // Declared first (lowest z), so the apply button and color dots keep their own clicks.
        MouseArea {
            id: cellMouse
            anchors.fill: parent
            // Hand focus to the view so arrow keys keep working after a mouse selection.
            onClicked: {
                root.grid.currentIndex = root.index
                root.grid.forceActiveFocus()
            }
            onDoubleClicked: root.openDetail()
        }
        // Clip keeps the hover/selection zoom inside the cell.
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
                // VideoOutput is transparent before the first frame; the still stays underneath until one lands.
                opacity: status === Image.Ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad } }
                // For unselected animated cells this static first-frame thumbnail IS the cell; it stays under the movie until it fades in.
            }
            // AnimatedImage cannot use QQuickImageProvider: it loads the real file URL; the Loader keeps the QMovie alive only for the selected, visible cell.
            Loader {
                id: animatedLoader
                anchors.fill: parent
                active: root.animatedActive && root.animatedArmed
                onActiveChanged: if (!active)
                    root.animatedArmed = false
                sourceComponent: Component {
                    AnimatedImage {
                        fillMode: Image.PreserveAspectCrop
                        asynchronous: true
                        smooth: true
                        sourceSize: Qt.size(root.animatedFrameWidth, 0)
                        source: root.app.fileUrl(root.path)
                        playing: true
                        scale: root.hovered ? 1.14 : (root.isSelected ? 1.07 : 1.0)
                        Behavior on scale {
                            SpringAnimation {
                                spring: 8.0
                                damping: 0.55
                                mass: 1.0
                            }
                        }
                        opacity: status === Image.Ready ? 1.0 : 0.0
                        Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad } }
                    }
                }
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
                    || thumbnailImage.status === Image.Error)
        }
        ToolButton {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Kirigami.Units.smallSpacing
            icon.name: "preferences-desktop-wallpaper"
            display: AbstractButton.IconOnly
            visible: root.isSelected
            // Not a tab stop: a stray Tab+Enter would apply a wallpaper per cell; the keyboard applies with Enter on the grid.
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
