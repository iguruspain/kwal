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

    implicitWidth: 160
    implicitHeight: 160
    background: Item {}

    readonly property bool isCurrent: root.path === root.app.currentWallpaper

    activeFocusOnTab: false
    highlighted: hovered
    onClicked: root.app.setWallpaper(root.path)

    contentItem: Item {
        Image {
            id: thumbnailImage
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            source: root.thumbnail
            asynchronous: true
            fillMode: Image.PreserveAspectFit
            sourceSize: Qt.size(512, 512)
        }
        Kirigami.Icon {
            anchors.centerIn: parent
            source: "image-x-generic"
            color: Kirigami.Theme.disabledTextColor
            visible: root.thumbnail.toString() === "" || thumbnailImage.status === Image.Error
        }
        Kirigami.Icon {
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: Kirigami.Units.mediumSpacing
            source: "checkmark"
            color: Kirigami.Theme.highlightColor
            visible: root.isCurrent
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
