import QtQuick
import QtQuick.Controls
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
        qsTr("Show color categories"),
        qsTr("Also apply to the lock screen"),
        qsTr("Resize videos"),
        qsTr("Target height"),
        qsTr("Frame rate")
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
                checked: page.app.colorCategoriesEnabled
                onToggled: page.app.colorCategoriesEnabled = checked
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                text: qsTr("Application")
                level: 3
            }

            KirigamiDelegates.SwitchSubtitleDelegate {
                Layout.fillWidth: true
                text: qsTr("Also apply to the lock screen")
                subtitle: qsTr("Write the applied wallpaper to the KScreenLock config too")
                checked: page.app.lockscreen
                onToggled: page.app.lockscreen = checked
            }

            Kirigami.Heading {
                Layout.fillWidth: true
                text: qsTr("Video resize")
                level: 3
            }

            KirigamiDelegates.SwitchSubtitleDelegate {
                Layout.fillWidth: true
                text: qsTr("Resize videos")
                subtitle: qsTr("Create a scaled cache copy of videos instead of applying the original")
                checked: page.app.resizeEnabled
                onToggled: page.app.resizeEnabled = checked
            }

            Kirigami.FormLayout {
                SpinBox {
                    Kirigami.FormData.label: qsTr("Target height")
                    from: 480
                    to: 4320
                    stepSize: 120
                    value: page.app.resizeHeight
                    enabled: page.app.resizeEnabled
                    onValueModified: page.app.resizeHeight = value
                }

                SpinBox {
                    Kirigami.FormData.label: qsTr("Frame rate")
                    from: 0
                    to: 240
                    stepSize: 5
                    textFromValue: function(value) {
                        return value === 0 ? qsTr("Original") : value
                    }
                    value: page.app.resizeFps
                    enabled: page.app.resizeEnabled
                    onValueModified: page.app.resizeFps = value
                }
            }
        }
    }
}
