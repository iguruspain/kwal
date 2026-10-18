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
                fillMode: Image.Stretch
                scale: root.hovered ? 1.14 : (root.isSelected ? 1.06 : 1.0)
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
            // Replaces the checkmark: appears on the selected cell and applies it.
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
            anchors.margins: Kirigami.Units.smallSpacing
            width: badgeLabel.implicitWidth
            height: badgeLabel.implicitHeight
            radius: Kirigami.Units.smallSpacing
            color: Kirigami.Theme.alternateBackgroundColor
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
