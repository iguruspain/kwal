import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KirigamiDelegates

Kirigami.ScrollablePage {
    id: page

    title: qsTr("Wallpapers")

    required property AppController app

    // Setting labels exposed to the settings drawer search; keep in sync
    // with the controls below.
    property var searchKeys: [
        qsTr("Show color dots (matugen)"),
        qsTr("Show color categories")
    ]

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Heading {
                Layout.fillWidth: true
                text: qsTr("Colors")
                level: 3
            }

            KirigamiDelegates.SwitchSubtitleDelegate {
                Layout.fillWidth: true
                text: qsTr("Show color dots (matugen)")
                subtitle: qsTr("Show the matugen source colors of the selected wallpaper")
                enabled: page.app.matugenAvailable
                checked: page.app.matugenDotsEnabled
                onToggled: page.app.matugenDotsEnabled = checked
            }

            KirigamiDelegates.SwitchSubtitleDelegate {
                Layout.fillWidth: true
                text: qsTr("Show color categories")
                subtitle: qsTr("Show the color category chips in the wallpapers search")
                enabled: page.app.matugenAvailable
                checked: page.app.colorCategoriesEnabled
                onToggled: page.app.colorCategoriesEnabled = checked
            }
        }
    }
}
