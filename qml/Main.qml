import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

Kirigami.ApplicationWindow {
    id: window
    readonly property bool enableTransparency: true
    readonly property AppController app: AppController {}
    readonly property color appBackground: enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
        : Kirigami.Theme.backgroundColor

    width: 960
    height: 660
    minimumWidth: 700
    minimumHeight: 400
    visible: true
    color: "transparent"
    title: qsTr("Kwal")

    FolderDialog {
        id: addFolderDialog
        title: qsTr("Choose a Wallpaper Folder")
        currentFolder: Qt.resolvedUrl(window.app.currentFolderPath)
        onAccepted: window.app.addFolder(selectedFolder.toString())
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
    //         icon.name: "utilities-system-tools"
    //         checked: window.pageStack.currentIndex === 1
    //         onClicked: window.pageStack.currentIndex = 1
    //     }
    //     Kirigami.NavigationTabButton {
    //         text: qsTr("Ulauncher")
    //         icon.name: "application-x-executable"
    //         checked: window.pageStack.currentIndex === 2
    //         onClicked: window.pageStack.currentIndex = 2
    //     }
    // }

    header: Rectangle {
        color: appBackground
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
                    currentIndex: window.pageStack.currentIndex

                    TabButton {
                        text: qsTr("Wallpapers")
                        checked: window.pageStack.currentIndex === 0
                        onClicked: window.pageStack.currentIndex = 0
                    }
                    TabButton {
                        text: qsTr("Fastfetch")
                        checked: window.pageStack.currentIndex === 1
                        onClicked: window.pageStack.currentIndex = 1
                    }
                    TabButton {
                        text: qsTr("Ulauncher")
                        checked: window.pageStack.currentIndex === 2
                        onClicked: window.pageStack.currentIndex = 2
                    }
                }
                Item { Layout.fillWidth: true }
                // ToolButton {
                //     icon.name: "preferences-system-symbolic"
                //     Layout.preferredWidth: Kirigami.Units.gridUnit * 2
                //     onClicked: {console.log("test")}    
                // }
            }
        }
    }

    pageStack.columnView.columnResizeMode: KirigamiLayouts.ColumnView.SingleColumn

    pageStack.initialPage: WallpapersPage {
        app: window.app
        addFolderDialog: addFolderDialog

        globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None
        background: Rectangle {
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
        }
    }
    Component { id: fastfetchPage; PlaceholderPage { toolName: "Fastfetch" } }
    Component { id: ulauncherPage; PlaceholderPage { toolName: "Ulauncher" } }


    Connections {
        target: window.app
        function onWallpaperSetResult(success, message) {
            window.showPassiveNotification(message, success ? "short" : "long")
        }
    }
    Component.onCompleted: {
        window.pageStack.push(fastfetchPage.createObject(window))
        window.pageStack.push(ulauncherPage.createObject(window))
        window.pageStack.currentIndex = 0
    }
}
