import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
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

    // Returns whether a drawer section matches the settings search: the query
    // must be contained in the section's page title or in any of the page's
    // setting labels (searchKeys).
    function sectionMatches(query, index) {
        if (query === "") {
            return true;
        }
        const page = settingsWindow.pageStack.items[index];
        if (!page) {
            return true;
        }
        const q = query.toLowerCase();
        if (page.title.toLowerCase().includes(q)) {
            return true;
        }
        const keys = page.searchKeys ?? [];
        for (const key of keys) {
            if (key.toLowerCase().includes(q)) {
                return true;
            }
        }
        return false;
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: globalDrawer

        // "Collapsible Sidebar Drawer" pattern (Kirigami Gallery, BaseApp).
        modal: false
        collapsible: true
        collapsed: false

        // Settings search; filters the sections below (see drawerMatches).
        header: ToolBar {
            visible: !globalDrawer.collapsed
            contentItem: RowLayout {
                Kirigami.SearchField {
                    id: drawerSearch
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search settings…")
                }
            }
        }

        actions: [
            Kirigami.Action {
                text: qsTr("General")
                icon.name: "preferences-system"
                visible: settingsWindow.sectionMatches(drawerSearch.text, 0)
                checked: settingsWindow.pageStack.currentIndex === 0
                onTriggered: settingsWindow.pageStack.currentIndex = 0
            },
            Kirigami.Action {
                text: qsTr("Wallpapers")
                icon.name: "preferences-desktop-wallpaper"
                visible: settingsWindow.sectionMatches(drawerSearch.text, 1)
                checked: settingsWindow.pageStack.currentIndex === 1
                onTriggered: settingsWindow.pageStack.currentIndex = 1
            },
            Kirigami.Action {
                text: qsTr("Fastfetch")
                icon.name: "utilities-terminal"
                visible: settingsWindow.sectionMatches(drawerSearch.text, 2)
                checked: settingsWindow.pageStack.currentIndex === 2
                onTriggered: settingsWindow.pageStack.currentIndex = 2
            },
            Kirigami.Action {
                text: qsTr("Starship")
                icon.name: "utilities-terminal"
                visible: settingsWindow.sectionMatches(drawerSearch.text, 3)
                checked: settingsWindow.pageStack.currentIndex === 3
                onTriggered: settingsWindow.pageStack.currentIndex = 3
            },
            Kirigami.Action {
                text: qsTr("Ulauncher")
                icon.name: "ulauncher-indicator-symbolic"
                visible: settingsWindow.sectionMatches(drawerSearch.text, 4)
                checked: settingsWindow.pageStack.currentIndex === 4
                onTriggered: settingsWindow.pageStack.currentIndex = 4
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
    Component { id: starshipSettingsPage; SettingsPlaceholderPage { sectionName: qsTr("Starship"); iconName: "utilities-terminal"; background: Rectangle { color: settingsWindow.pageBackground } } }    
    Component { id: ulauncherSettingsPage; SettingsPlaceholderPage { sectionName: qsTr("Ulauncher"); iconName: "ulauncher"; background: Rectangle { color: settingsWindow.pageBackground } } }

    Component.onCompleted: {
        settingsWindow.pageStack.push(wallpapersSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.push(fastfetchSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.push(starshipSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.push(ulauncherSettingsPage.createObject(settingsWindow))
        settingsWindow.pageStack.currentIndex = 0
    }
}
