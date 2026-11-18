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

    readonly property var tabs: (function () {
        const tabs = [
            { title: qsTr("Wallpapers"), icon: "preferences-desktop-wallpaper", page: 0 },
            { title: qsTr("Fastfetch"), icon: "utilities-terminal", page: 1, available: app.fastfetchAvailable },
            { title: qsTr("Starship"), icon: "utilities-terminal", page: 2, available: app.starshipAvailable },
            { title: qsTr("Ulauncher"), icon: "ulauncher-indicator-symbolic", page: 3, available: app.ulauncherAvailable }
        ];
        return tabs.filter(function (t) { return t.available !== false; });
    })()

    // Pages are created lazily, so stack position != tab id; map by object identity against currentItem.
    function currentTabIndex() {
        const current = window.pageStack.currentItem;
        for (let i = 0; i < window.tabs.length; i++) {
            const page = window.tabs[i].page === 0
                ? window.pages[0]
                : window.pages[window.tabs[i].page];
            if (page !== undefined && page === current)
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

    property var settingsWindow

    Component {
        id: settingsWindowComponent

        SettingsWindow {
            app: window.app
            ulauncher: window.app.ulauncher
        }
    }

    header: Rectangle {
        color: window.appBackground
        // A bare Rectangle has implicitHeight 0: set it so the header pushes pageStack down instead of the page ToolBar overlapping this tab row.
        implicitHeight: tabBar.implicitHeight + Kirigami.Units.largeSpacing
        ColumnLayout {
            anchors.fill: parent
            RowLayout {
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Item { Layout.fillWidth: true }
                TabBar {
                    id: tabBar
                    currentIndex: window.currentTabIndex()

                    Repeater {
                        model: window.tabs
                        delegate: TabButton {
                            required property int index
                            required property var modelData
                            text: modelData.title
                            icon.name: modelData.icon
                            checked: window.currentTabIndex() === index
                            onClicked: window.activateTab(modelData.page)
                        }
                    }
                }
                Item { Layout.fillWidth: true }
                // Must stay in the RowLayout: as a direct child of the header Rectangle it sits top-left and out of tab order.
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
        }
    }

    pageStack.columnView.columnResizeMode: KirigamiLayouts.ColumnView.SingleColumn

    pageStack.initialPage: WallpapersPage {
        id: wallpapersPage
        app: window.app
        addFolderDialog: folderPicker
        // PageRow keeps non-current pages visible: gate the grid video on this, not page.visible.
        pageActive: window.pageStack.currentIndex === 0
        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
        background: Rectangle {
            color: window.appBackground
        }
    }
    Component {
        id: fastfetchPage
        FastfetchPage {
            id: fastfetchPageInstance
            app: window.app
            pageActive: window.pageStack.currentItem === fastfetchPageInstance
        }
    }
    Component {
        id: starshipPage
        StarshipPage {
            id: starshipPageInstance
            app: window.app
            pageActive: window.pageStack.currentItem === starshipPageInstance
        }
    }
    Component {
        id: ulauncherPage
        UlauncherPage {
            id: ulauncherPageInstance
            app: window.app
            controller: window.app.ulauncher
            pageActive: window.pageStack.currentItem === ulauncherPageInstance
        }
    }

    // Lazily created tool pages keyed by tab `page` id; WebEngineViews in the Starship/Ulauncher previews exist only after first activation.
    property var pages: ({})
    // Kirigami.PageStack has no get(Item); track push order so activateTab can jump via currentIndex.
    property var stackIds: [0]

    function activateTab(id) {
        let page = window.pages[id];
        if (page === undefined) {
            switch (id) {
            case 1: page = fastfetchPage.createObject(window); break;
            case 2: page = starshipPage.createObject(window); break;
            case 3: page = ulauncherPage.createObject(window); break;
            }
            if (page === null)
                return;
            window.pages[id] = page;
            window.stackIds.push(id);
            window.pageStack.push(page);
        } else {
            window.pageStack.currentIndex = window.stackIds.indexOf(id);
        }
    }

    Connections {
        target: window.app
        function onWallpaperSetResult(success, message) {
            window.showPassiveNotification(message, success ? "short" : "long")
        }
    }
    Component.onCompleted: {
        window.pages[0] = wallpapersPage;
        window.pageStack.currentIndex = 0;
    }
}
