import QtQuick
import QtWebEngine

// The preview HTML is self-contained (inline CSS + fonts), so each update is a full reload.
Item {
    id: root

    property string html: ""

    WebEngineView {
        id: view
        anchors.fill: parent
        focus: false
        backgroundColor: "transparent"
        settings.localContentCanAccessFileUrls: true
        Component.onCompleted: if (root.html !== "")
            loadHtml(root.html, "file:///")
    }

    onHtmlChanged: if (html !== "")
        view.loadHtml(html, "file:///")
}
