import QtQuick
import QtWebEngine

// Transparent WebEngine background so the launcher window shadow paints over the page background; the document is self-contained, so each update is a full reload.
Rectangle {
    id: root
    color: "transparent"

    property string html: ""
    // Launcher window width inside the generated HTML, in px.
    property real launcherWidth: 650
    // Natural mockup size: launcherWidth plus the 50px scaler margins; height is measured from the rendered document after each load.
    readonly property real contentWidth: launcherWidth + 100
    property real contentHeight: 450
    readonly property real fitScale: {
        if (width <= 0 || height <= 0)
            return 0.75
        return Math.min(width / contentWidth, height / contentHeight) * 0.95
    }

    WebEngineView {
        id: view
        anchors.fill: parent
        focus: false
        backgroundColor: "transparent"
        settings.localContentCanAccessFileUrls: true
        Component.onCompleted: if (root.html !== "")
            loadHtml(root.html, "file:///")
        // Qt 6.12's WebEngineView has no loadFinished signal; the loading
        // property going false is the load-completed equivalent.
        onLoadingChanged: if (!view.loading)
            view.runJavaScript(
                "var el = document.querySelector('.scaler');el ? el.offsetHeight : 0",
                (h) => {
                    if (h > 0)
                        root.contentHeight = h
                })
    }

    onHtmlChanged: if (html !== "")
        view.loadHtml(html, "file:///")
}
