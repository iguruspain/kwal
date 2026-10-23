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
        qsTr("Frame rate"),
        qsTr("Folder"),
        qsTr("Generate resizes")
    ]

    // Direct child of the ScrollablePage (no contentItem override): the page
    // anchors left/right to its scroll area; the top anchor keeps the content
    // flush with the top so the page scrolls when it overflows.
    ColumnLayout {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: Kirigami.Units.largeSpacing
        }
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
            subtitle: qsTr("Apply the scaled copy instead of the original video")
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
                onValueModified: page.app.resizeFps = value
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            ComboBox {
                id: folderCombo
                Layout.fillWidth: true
                model: page.app.videoFolders
                enabled: page.app.videoFolders.length > 0
            }

            Button {
                text: page.app.wallpapersInfo.resizing ? qsTr("Generating…") : qsTr("Generate resizes")
                enabled: page.app.videoFolders.length > 0 && !page.app.wallpapersInfo.resizing
                onClicked: {
                    const index = folderCombo.currentIndex
                    if (index >= 0 && index < folderCombo.model.length) {
                        page.app.generateVideoResizes(folderCombo.model[index])
                    }
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, page.app.wallpapersInfo.resizeTotal)
            value: page.app.wallpapersInfo.resizeCompleted
            visible: page.app.wallpapersInfo.resizing
        }
    }
}
