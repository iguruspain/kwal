import QtQuick
import QtQuick.Controls
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

// Shared shell for the tool pages: background, operation-result dialog, header suppression.
Kirigami.Page {
    id: page

    property AppController app

    // The PageRow keeps every pushed page visible, so `visible` never toggles on tab switch; Main.qml binds this.
    property bool pageActive: true

    property string resultTitle: ""

    readonly property color pageBackground: app.enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, app.transparencyLevel / 100.0)
        : Kirigami.Theme.backgroundColor

    background: Rectangle {
        color: page.pageBackground
    }

    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    // The Connections block stays in the subclass: the target controller differs per page.
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
