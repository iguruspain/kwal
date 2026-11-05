import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// The matugen source-color dots overlaid at the bottom-left of a wallpaper's
// media — used by both the grid's selected cell and the detail drawer. The
// selected dot feeds the %scolor%/%sc% placeholders of the custom commands.
// The caller sets placement (anchors) and `visible` (the grid additionally
// gates on selection); only `dotSize` differs between the two surfaces.
Rectangle {
    id: root

    required property AppController app
    required property var colors

    property real dotSize: Kirigami.Units.gridUnit

    color: "transparent"
    radius: Kirigami.Units.smallSpacing
    width: dotsRow.implicitWidth + Kirigami.Units.smallSpacing * 2
    height: dotsRow.implicitHeight + Kirigami.Units.smallSpacing * 2

    RowLayout {
        id: dotsRow
        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: root.colors
            delegate: ColorDot {
                required property string modelData
                required property int index
                id: sourceDot
                dotSize: root.dotSize
                color: sourceDot.modelData
                selected: root.app.selectedSourceColorIndex === sourceDot.index
                onClicked: root.app.selectedSourceColorIndex = sourceDot.index
            }
        }
    }
}
