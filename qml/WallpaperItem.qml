import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

ItemDelegate {
    id: root
    required property AppController app
    required property string path
    required property string name
    required property url thumbnail
    required property string extension
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
                opacity: status === Image.Ready ? 1.0 : 0.0
                Behavior on opacity { NumberAnimation { duration: Kirigami.Units.longDuration; easing.type: Easing.InOutQuad } }
            }
        }
        Kirigami.Icon {
            anchors.centerIn: parent
            source: "image-x-generic"
            color: Kirigami.Theme.disabledTextColor
            visible: root.thumbnail.toString() === "" || thumbnailImage.status === Image.Error
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
    }
}
