import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// Shared shell for the tool tabs (Fastfetch, Starship, Ulauncher). Each of
// those pages is a Kirigami.Page with the same chrome: an app-wide
// controller reference, a transparency-aware background (also reused by the
// page's own panes through pageBackground), no page-level header, and a
// modal dialog that surfaces the controller's operationResult. Only the
// background, the result dialog and the header suppression live here; the
// per-tool controller (a distinct C++ type per tab) and its page body stay
// in the subclass.
Kirigami.Page {
    id: page

    // App-wide controller (folders / settings / transparency); subclasses
    // bind it to the window's controller.
    property AppController app

    // Title of the operation-result dialog; set by the subclass.
    property string resultTitle: ""

    // Transparency-aware page background; the page's panes reference this so
    // they match the window background exactly, as on WallpapersPage.
    readonly property color pageBackground: app.enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, app.transparencyLevel / 100.0)
        : Kirigami.Theme.backgroundColor

    background: Rectangle {
        color: page.pageBackground
    }

    // Hide the page-level navigation header (back/forward + title): the
    // window tab bar is the only chrome, matching WallpapersPage.
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    // Opens the result dialog; the subclass calls this from its
    // onOperationResult handler (the target controller differs per page, so
    // the Connections block stays in the subclass).
    function showResult(message) {
        resultDialog.message = message
        resultDialog.visible = true
    }

    Dialog {
        id: resultDialog
        title: page.resultTitle
        modal: true
        visible: false
        standardButtons: Dialog.Ok
        property string message: ""
        contentItem: Label {
            text: resultDialog.message
            wrapMode: Text.WordWrap
        }
    }
}
