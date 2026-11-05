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

    // The tab bar index backing the page that is currently shown. Pages are
    // created lazily, so a page's position in the stack is not its tab id;
    // the only reliable mapping is object identity against currentItem.
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

    // Created tool pages, keyed by the tab's `page` id. Only the Wallpapers
    // page (index 0) exists at startup; the Fastfetch/Starship/Ulauncher
    // pages — and the WebEngineViews inside the Starship and Ulauncher
    // previews — are built on first tab activation.
    property var pages: ({})
    // Tab ids in page-stack order. The Wallpapers page is the initialPage
    // (stack index 0); every lazily-created tool page is appended, so this
    // array's index is the page's stack position. Kirigami.PageStack has no
    // get(Item) (it is ColumnView-backed: get(index) returns the item), so we
    // track the order ourselves and jump with currentIndex.
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
        // The Wallpapers page is the initialPage (index 0); register it so
        // the tab mapping can find it. Tool pages load lazily via activateTab.
        window.pages[0] = wallpapersPage;
        window.pageStack.currentIndex = 0;
    }
}
