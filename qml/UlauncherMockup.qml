import QtQuick
import QtWebEngine

// Renders the Ulauncher theme preview HTML in a WebEngine view. The document is
// self-contained, so each update is a full reload.
Item {
    id: root

    // The HTML document to display.
    property string html: ""

    WebEngineView {
        id: view
        anchors.fill: parent
        focus: false
        Component.onCompleted: if (root.html !== "")
            loadHtml(root.html)
    }

    onHtmlChanged: if (html !== "")
        view.loadHtml(html)
}
