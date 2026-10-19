import QtQuick
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

pragma ComponentBehavior: Bound

Kirigami.ApplicationWindow {
    id: settingsWindow

    title: qsTr("Kwal Settings")
    width: 800
    height: 600
    minimumWidth: 640
    minimumHeight: 400
    color: "transparent"

    required property AppController app

    // Mirrors the main window so toggling transparency updates both live.
    readonly property color pageBackground: app.enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
        : Kirigami.Theme.backgroundColor

    globalDrawer: Kirigami.GlobalDrawer {
        id: globalDrawer

        // "Collapsible Sidebar Drawer" pattern (Kirigami Gallery, BaseApp).
        modal: false
        collapsible: true
        collapsed: false

        actions: [
            Kirigami.Action {
                text: qsTr("General")
                icon.name: "preferences-system"
                checked: settingsWindow.pageStack.currentIndex === 0
                onTriggered: settingsWindow.pageStack.currentIndex = 0
            },
            Kirigami.Action {
                text: qsTr("Wallpapers")
                icon.name: "preferences-desktop-wallpaper"
                checked: settingsWindow.pageStack.currentIndex === 1
                onTriggered: settingsWindow.pageStack.currentIndex = 1
            },
            Kirigami.Action {
                text: qsTr("Fastfetch")
                icon.name: "utilities-terminal"
                checked: settingsWindow.pageStack.currentIndex === 2
                onTriggered: settingsWindow.pageStack.currentIndex = 2
            },
            Kirigami.Action {
                text: qsTr("Ulauncher")
                icon.name: "application-x-executable"
                checked: settingsWindow.pageStack.currentIndex === 3
                onTriggered: settingsWindow.pageStack.currentIndex = 3
            }
        ]
    }

    pageStack.columnView.columnResizeMode: KirigamiLayouts.ColumnView.SingleColumn

    pageStack.initialPage: GeneralSettingsPage {
        app: settingsWindow.app
        background: Rectangle {
            color: settingsWindow.pageBackground
        }
    }
    Component { id: wallpapersSettingsPage; SettingsPlaceholderPage { sectionName: qsTr("Wallpapers"); iconName: "preferences-desktop-wallpaper"; background: Rectangle { color: settingsWindow.pageBackground } } }
    Component { id: fastfetchSettingsPage; SettingsPlaceholderPage { sectionName: qsTr("Fastfetch"); iconName: "utilities-terminal"; background: Rectangle { color: settingsWindow.pageBackground } } }
    Component { id: ulauncherSettingsPage; SettingsPlaceholderPage { sectionName: qsTr("Ulauncher"); iconName: "application-x-executable"; background: Rectangle { color: settingsWindow.pageBackground } } }

    Component.onCompleted: {
        settingsWindow.pageStack.push(wallpapersSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.push(fastfetchSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.push(ulauncherSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.currentIndex = 0
    }
}
