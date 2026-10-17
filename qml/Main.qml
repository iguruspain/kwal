import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.ApplicationWindow {
    id: root

    width: Kirigami.Units.gridUnit * 40
    height: Kirigami.Units.gridUnit * 30
    minimumWidth: Kirigami.Units.gridUnit * 32
    minimumHeight: Kirigami.Units.gridUnit * 20
    visible: true
    title: qsTr("Kwal")

    pageStack.initialPage: Kirigami.Page {
        title: qsTr("Kwal")

        contentItem: Item {
            Kirigami.PlaceholderMessage {
                anchors.centerIn: parent
                icon.name: "preferences-desktop-wallpaper"
                text: qsTr("Welcome to Kwal")
                explanation: qsTr("Manage your Plasma wallpaper and tint your favorite tools with palettes extracted from it.")
            }
        }
    }
}
