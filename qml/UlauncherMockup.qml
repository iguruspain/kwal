import QtQuick
import QtWebEngine

// Renders the Ulauncher theme preview HTML in a WebEngine view on a
// transparent background, so the launcher window's shadow is painted over
// the page background. The document is self-contained, so each update is a
// full reload.
Rectangle {
    id: root
    color: "transparent"

    // The HTML document to display.
    property string html: ""
    // Width of the launcher window inside the generated HTML in px; the page
    // binds it to the controller's preview window width.
    property real launcherWidth: 650
    // Natural size of the generated mockup in HTML units: the launcher
    // window plus the 50px scaler margins. The width follows launcherWidth;
    // the height is measured from the rendered document after each load, so
    // the mockup resizes with the number of sample rows.
    readonly property real contentWidth: launcherWidth + 100
    property real contentHeight: 450
    // Scale that fits the natural-size mockup into this rectangle with a
    // small margin, as the old app computed from the pane size.
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
