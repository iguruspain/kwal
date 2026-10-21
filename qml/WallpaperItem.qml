import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    readonly property bool isSelected: root.path === root.app.selectedWallpaper
    // Whether the wallpapers page is currently visible; passed down by the page.
    property bool pageVisible: true
    // The selected video cell plays live (muted, looping) instead of the still.
    // Media only exists while the cell is the active video, so at most one
    // MediaPlayer/VideoOutput exists at a time; switching tabs, folders or
    // selection stops playback and releases the GPU resources.
    readonly property bool videoActive: root.isVideo && root.isSelected && root.pageVisible

    // Copies a source color's hex to the clipboard and notifies the user,
    // like ColorDot on the tool pages.
    function copyDotColor(hex) {
        dotClipboard.text = hex
        dotClipboard.selectAll()
        dotClipboard.copy()
        // qmllint disable unqualified
        const win = applicationWindow()
        // qmllint enable unqualified
        if (win)
            win.showPassiveNotification(
                qsTr("Copied: %1").arg(hex), "short")
    }

    activeFocusOnTab: false
    hoverEnabled: true
    onClicked: root.grid.currentIndex = index

    contentItem: Item {
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
                fillMode: Image.Stretch //Image.PreserveAspectCrop 
                scale: root.hovered ? 1.14 : (root.isSelected ? 1.07 : 1.0)
                Behavior on scale {
                    SpringAnimation {
                        spring: 8.0
                        damping: 0.55
                        mass: 1.0
                    }
                }
                visible: !root.videoActive
                opacity: status === Image.Ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad } }
            }
            Loader {
                id: videoLoader
                anchors.fill: parent
                active: root.videoActive
                asynchronous: false
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
            visible: !root.videoActive && (root.thumbnail.toString() === "" || thumbnailImage.status === Image.Error)
        }
        ToolButton {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Kirigami.Units.smallSpacing
            icon.name: "dialog-ok-apply"
            display: AbstractButton.IconOnly
            visible: root.isSelected
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
        // The matugen source colors of the selected wallpaper, like the old
        // app's colorDotsContainer: bottom-left, above the extension badge row.
        Rectangle {
            id: colorDotsContainer
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            anchors.margins: Kirigami.Units.smallSpacing * 2
            radius: Kirigami.Units.smallSpacing
            visible: root.isSelected
                && root.app.matugenDotsEnabled
                && root.sourceColors.length > 0
            color: "transparent"
            // color: Kirigami.Theme.alternateBackgroundColor
            // border.color: Kirigami.Theme.highlightColor
            // border.width: 0.5
            width: colorDots.implicitWidth + Kirigami.Units.smallSpacing * 2
            height: colorDots.implicitHeight + Kirigami.Units.smallSpacing * 2

            RowLayout {
                id: colorDots
                anchors.centerIn: parent
                spacing: 2

                Repeater {
                    model: root.sourceColors
                    delegate: Rectangle {
                        id: sourceDot
                        required property string modelData
                        required property int index
                        width: Kirigami.Units.gridUnit * 0.8
                        height: width
                        radius: Kirigami.Units.smallSpacing
                        color: sourceDot.modelData
                        readonly property bool dotSelected: root.app.selectedSourceColorIndex === sourceDot.index
                        border.color: dotSelected
                            ? Kirigami.Theme.positiveTextColor
                            : Kirigami.Theme.textColor
                        border.width: dotSelected ? 2 : 1
                        ToolTip.text: sourceDot.modelData.toUpperCase()
                        ToolTip.visible: dotHover.hovered
                        HoverHandler {
                            id: dotHover
                        }
                        TapHandler {
                            acceptedButtons: Qt.LeftButton
                            onTapped: root.app.selectedSourceColorIndex = sourceDot.index
                        }
                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            onTapped: root.copyDotColor(sourceDot.modelData)
                        }
                    }
                }
            }
            // Invisible TextEdit used to copy a dot's hex color.
            TextEdit {
                id: dotClipboard
                visible: false
            }
        }
    }
}
