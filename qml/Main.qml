import QtQuick
import QtQuick.Dialogs
import org.kde.kirigami as Kirigami
import org.kde.kirigami.layouts as KirigamiLayouts

Kirigami.ApplicationWindow {
    id: window
    readonly property AppController app: AppController {}

    width: 960
    height: 600
    minimumWidth: 700
    minimumHeight: 400
    visible: true
    title: qsTr("Kwal")

    FolderDialog {
        id: addFolderDialog
        title: qsTr("Choose a Wallpaper Folder")
        currentFolder: Qt.resolvedUrl(window.app.currentFolderPath)
        onAccepted: window.app.addFolder(selectedFolder.toString())
    }

    footer: Kirigami.NavigationTabBar {
        Kirigami.NavigationTabButton {
            text: qsTr("Wallpapers")
            icon.name: "preferences-desktop-wallpaper"
            checked: window.pageStack.currentIndex === 0
            onClicked: window.pageStack.currentIndex = 0
        }
        Kirigami.NavigationTabButton {
            text: qsTr("Fastfetch")
            icon.name: "utilities-system-tools"
            checked: window.pageStack.currentIndex === 1
            onClicked: window.pageStack.currentIndex = 1
        }
        Kirigami.NavigationTabButton {
            text: qsTr("Ulauncher")
            icon.name: "application-x-executable"
            checked: window.pageStack.currentIndex === 2
            onClicked: window.pageStack.currentIndex = 2
        }
    }

    pageStack.columnView.columnResizeMode: KirigamiLayouts.ColumnView.SingleColumn

    pageStack.initialPage: WallpapersPage {
        app: window.app
        addFolderDialog: addFolderDialog
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
