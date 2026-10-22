import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

Kirigami.ApplicationWindow {
    id: window
    readonly property AppController app: AppController {}
    readonly property color appBackground: app.enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, app.transparencyLevel / 100.0)
        : Kirigami.Theme.backgroundColor

    // Header tabs; tools not installed on the system are excluded from the
    // model so the bar never renders an empty slot for them.
    readonly property var tabs: (function () {
        const tabs = [
            { title: qsTr("Wallpapers"), icon: "preferences-desktop-wallpaper", page: 0 },
            { title: qsTr("Fastfetch"), icon: "utilities-terminal", page: 1, available: app.fastfetchAvailable },
            { title: qsTr("Starship"), icon: "utilities-terminal", page: 2, available: app.starshipAvailable },
            { title: qsTr("Ulauncher"), icon: "ulauncher-indicator-symbolic", page: 3, available: app.ulauncherAvailable }
        ];
        return tabs.filter(function (t) { return t.available !== false; });
    })()

    // Index of the tab backing a pageStack page; the model skips pages of
    // tools that are not installed.
    function tabBarIndex(pageIndex) {
        for (let i = 0; i < window.tabs.length; i++) {
            if (window.tabs[i].page === pageIndex)
                return i;
        }
        return 0;
    }

    width: app.defaultWindowWidth
    height: app.defaultWindowHeight
    minimumWidth: 700
    minimumHeight: 400
    visible: true
    color: "transparent"
    title: qsTr("Kwal")

    FolderDialog {
        id: folderPicker
        title: qsTr("Choose a Wallpaper Folder")
        currentFolder: Qt.resolvedUrl(window.app.currentFolderPath)
        onAccepted: window.app.addFolder(selectedFolder.toString())
    }

    // Created lazily on first use so no second surface exists at startup.
    property var settingsWindow

    Component {
        id: settingsWindowComponent

        SettingsWindow {
            app: window.app
            ulauncher: window.app.ulauncher
        }
    }

    // footer: Kirigami.NavigationTabBar {
    //     Kirigami.NavigationTabButton {
    //         text: qsTr("Wallpapers")
    //         icon.name: "preferences-desktop-wallpaper"
    //         checked: window.pageStack.currentIndex === 0
    //         onClicked: window.pageStack.currentIndex = 0
    //     }
    //     Kirigami.NavigationTabButton {
    //         text: qsTr("Fastfetch")
    //         icon.name: "utilities-terminal"
    //         checked: window.pageStack.currentIndex === 1
    //         onClicked: window.pageStack.currentIndex = 1
    //     }
    //     Kirigami.NavigationTabButton {
    //         text: qsTr("Ulauncher")
    //         icon.name: "ulauncher-indicator-symbolic"
    //         checked: window.pageStack.currentIndex === 2
    //         onClicked: window.pageStack.currentIndex = 2
    //     }
    // }

    header: Rectangle {
        color: window.appBackground
        // Reserve vertical space: a bare Rectangle has implicitHeight 0, so the
        // window header would not push the pageStack down and the page's own
        // ToolBar would overlap this tab row.
        implicitHeight: tabBar.implicitHeight + Kirigami.Units.largeSpacing
        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }
                TabBar {
                    id: tabBar
                    currentIndex: window.tabBarIndex(window.pageStack.currentIndex)

                    Repeater {
                        model: window.tabs
                        delegate: TabButton {
                            text: modelData.title
                            icon.name: modelData.icon
                            checked: window.pageStack.currentIndex === modelData.page
                            onClicked: window.pageStack.currentIndex = modelData.page
                        }
                    }
                }
                Item { Layout.fillWidth: true }
            }
        }
        Item { Layout.fillWidth: true }
        ToolButton {
            id: menuButton
            icon.name: "open-menu-symbolic"
            onClicked: appMenu.open()

            Menu {
                id: appMenu
                y: menuButton.height

                MenuItem {
                    text: qsTr("Configure Kwal")
                    icon.name: "preferences-system-symbolic"
                    onTriggered: {
                        if (!window.settingsWindow)
                            window.settingsWindow = settingsWindowComponent.createObject(window)
                        window.settingsWindow.show()
                    }
                }
                MenuSeparator {}
                MenuItem {
                    text: qsTr("Quit")
                    icon.name: "application-exit"
                    onTriggered: Qt.quit()
                }
            }
        }
    }

    pageStack.columnView.columnResizeMode: KirigamiLayouts.ColumnView.SingleColumn

    pageStack.initialPage: WallpapersPage {
        id: wallpapersPage
        app: window.app
        addFolderDialog: folderPicker
        // The PageRow keeps non-current pages alive and visible, so the
        // video in the grid is gated on the page actually being the current
        // tab, not on page.visible.
        pageActive: window.pageStack.currentIndex === 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
        background: Rectangle {
            color: window.appBackground
        }
    }
    Component {
        id: fastfetchPage
        FastfetchPage {
            app: window.app
        }
    }
    Component { id: starshipPage; StarshipPage { app: window.app } }
    Component { id: ulauncherPage; UlauncherPage { app: window.app; controller: window.app.ulauncher } }


    Connections {
        target: window.app
        function onWallpaperSetResult(success, message) {
            window.showPassiveNotification(message, success ? "short" : "long")
        }
    }
    Component.onCompleted: {
        window.pageStack.push(fastfetchPage.createObject(window))
        window.pageStack.push(starshipPage.createObject(window))
        window.pageStack.push(ulauncherPage.createObject(window))
        window.pageStack.currentIndex = 0
    }
}
