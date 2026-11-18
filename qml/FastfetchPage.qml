import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

ToolPage {
    id: page
    title: qsTr("Fastfetch")
    resultTitle: qsTr("Fastfetch")

    readonly property FastfetchController controller: FastfetchController {}

    function shortPath(path) {
        const parts = String(path).split("/").filter(function (s) { return s.length > 0 })
        if (parts.length <= 2)
            return String(path)
        return "…" + "/" + parts[parts.length - 2] + "/" + parts[parts.length - 1]
    }

    FileDialog {
        id: imagePicker
        title: qsTr("Select an Image")
        currentFolder: StandardPaths.standardLocations(StandardPaths.PicturesLocation)[0]
        nameFilters: [
            qsTr("Images (*.png *.jpg *.jpeg *.bmp *.webp *.gif)")
        ]
        onAccepted: page.controller.selectFile(selectedFile.toString())
    }

    Connections {
        target: page.controller
        function onOperationResult(success, message) {
            page.showResult(message)
        }
    }

    // pageActive, not visible: the PageRow keeps pushed pages visible; this is the only moment the fastfetch config can have changed behind our back.
    onPageActiveChanged: {
        if (pageActive)
            page.controller.refreshConfigImage()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        Rectangle {
            id: leftPane
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: page.pageBackground
            radius: Kirigami.Units.largeSpacing
            border.color: Kirigami.Theme.highlightColor
            border.width: 1.5

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.gridUnit

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Label {
                        text: qsTr("Settings")
                        font.bold: true
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Select an image")
                    }
                    ToolButton {
                        icon.name: "edit-clear"
                        enabled: page.controller.selectedFile !== ""
                        ToolTip.text: qsTr("Clear selection")
                        ToolTip.visible: hovered
                        onClicked: page.controller.clearSelection()
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    TextField {
                        id: imageField
                        Layout.fillWidth: true
                        readOnly: true
                        text: page.controller.selectedFile === ""
                            ? qsTr("No image selected")
                            : page.shortPath(page.controller.selectedFile)
                    }
                    ToolButton {
                        icon.name: "document-open"
                        ToolTip.text: qsTr("Select an image")
                        ToolTip.visible: hovered
                        onClicked: imagePicker.open()
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Label {
                        text: qsTr("Tint color:")
                    }
                    Item { Layout.fillWidth: true }
                    ColorDot {
                        enablePicker: true
                        color: page.controller.draftColor
                        onAccepted: (hex) => { page.controller.draftColor = hex }
                    }
                }

                Item { Layout.fillHeight: true }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.largeSpacing

                    ToolButton {
                        icon.name: "document-save"
                        ToolTip.text: qsTr("Apply the selected image directly (no tint)")
                        ToolTip.visible: hovered
                        enabled: page.controller.selectedFile !== ""
                            && !page.controller.busy
                        onClicked: page.controller.applyOriginalImage()
                    }
                    ToolButton {
                        icon.name: page.controller.busy ? "" : "dialog-ok-apply"
                        ToolTip.text: page.controller.busy
                            ? qsTr("Applying the tinted image…")
                            : qsTr("Apply the tinted image")
                        ToolTip.visible: hovered
                        enabled: page.controller.selectedFile !== ""
                            && page.controller.draftColor !== "transparent"
                            && !page.controller.busy
                        onClicked: page.controller.applyTintedImage()
                        BusyIndicator {
                            anchors.centerIn: parent
                            width: 24
                            height: 24
                            running: page.controller.busy
                            visible: page.controller.busy
                        }
                    }
                    ToolButton {
                        icon.name: "document-revert"
                        ToolTip.text: qsTr("Restore the fastfetch config from the last backup")
                        ToolTip.visible: hovered
                        enabled: page.controller.hasBackup
                            && !page.controller.busy
                        onClicked: page.controller.restoreBackup()
                    }
                }
            }
        }

        Rectangle {
            id: rightPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"
            radius: Kirigami.Units.largeSpacing

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.gridUnit

                Label {
                    text: qsTr("Current (config):")
                    font.bold: true
                }

                Label {
                    Layout.fillWidth: true
                    font.italic: true
                    wrapMode: Text.WrapAnywhere
                    color: Kirigami.Theme.disabledTextColor
                    text: page.controller.configImage === ""
                        ? qsTr("No image found")
                        : page.shortPath(page.controller.configImage)
                }

                Item {
                    id: configPreviewArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 300
                    // Image shows only the first frame: animated sources (GIF/WebP) use AnimatedImage.
                    readonly property bool isAnimated:
                        page.controller.configImage.toLowerCase().endsWith(".gif")
                        || page.controller.configImage.toLowerCase().endsWith(".webp")
                    Image {
                        anchors.centerIn: parent
                        source: page.controller.configImageUrl
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        visible: page.controller.configImage !== "" && !configPreviewArea.isAnimated
                        width: Math.min(configPreviewArea.width, configPreviewArea.height)
                        height: Math.min(configPreviewArea.width, configPreviewArea.height)
                    }
                    AnimatedImage {
                        anchors.centerIn: parent
                        source: configPreviewArea.isAnimated ? page.controller.configImageUrl : ""
                        asynchronous: true
                        fillMode: Image.PreserveAspectFit
                        // AnimatedImage caches every frame in memory; 1280x0 caps stored frame width, zero height keeps the aspect ratio.
                        sourceSize: Qt.size(1280, 0)
                        playing: page.pageActive
                        width: Math.min(configPreviewArea.width, configPreviewArea.height)
                        height: Math.min(configPreviewArea.width, configPreviewArea.height)
                    }
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No configured image")
                        color: Kirigami.Theme.disabledTextColor
                        visible: page.controller.configImage === ""
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 300
                    spacing: Kirigami.Units.largeSpacing
                    visible: page.controller.previewsVisible

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 200
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: qsTr("Selected image:")
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            font.italic: true
                            wrapMode: Text.WrapAnywhere
                            color: Kirigami.Theme.disabledTextColor
                            text: page.shortPath(page.controller.selectedFile)
                        }

                        Item {
                            id: originalPreviewArea
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            readonly property bool isAnimated:
                                page.controller.selectedFile.toLowerCase().endsWith(".gif")
                                || page.controller.selectedFile.toLowerCase().endsWith(".webp")
                            Image {
                                anchors.centerIn: parent
                                source: page.controller.fileUrl(page.controller.selectedFile)
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                visible: !originalPreviewArea.isAnimated
                                width: Math.min(originalPreviewArea.width, originalPreviewArea.height)
                                height: Math.min(originalPreviewArea.width, originalPreviewArea.height)
                            }
                            AnimatedImage {
                                anchors.centerIn: parent
                                source: originalPreviewArea.isAnimated
                                    ? page.controller.fileUrl(page.controller.selectedFile)
                                    : ""
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                sourceSize: Qt.size(1280, 0)
                                playing: page.pageActive
                                width: Math.min(originalPreviewArea.width, originalPreviewArea.height)
                                height: Math.min(originalPreviewArea.width, originalPreviewArea.height)
                            }
                        }
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.minimumWidth: 200
                        spacing: Kirigami.Units.smallSpacing

                        Label {
                            text: qsTr("Tinted:")
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            font.italic: true
                            wrapMode: Text.WrapAnywhere
                            color: Kirigami.Theme.disabledTextColor
                            text: page.controller.draftColor === "transparent"
                                ? qsTr("No tint selected")
                                : page.shortPath(page.controller.tintedImagePathFor(page.controller.selectedFile))
                        }

                        Item {
                            id: tintedPreviewArea
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            // A tinted GIF/WebP is generated as one animated file; before a tint exists the selected file shows instead.
                            readonly property bool isAnimated:
                                (page.controller.tintedPreviewPath !== ""
                                    ? page.controller.tintedPreviewPath
                                    : page.controller.selectedFile)
                                    .toLowerCase().endsWith(".gif")
                                || (page.controller.tintedPreviewPath !== ""
                                    ? page.controller.tintedPreviewPath
                                    : page.controller.selectedFile)
                                    .toLowerCase().endsWith(".webp")
                            Image {
                                anchors.centerIn: parent
                                source: page.controller.tintedPreviewUrl !== ""
                                    ? page.controller.tintedPreviewUrl
                                    : page.controller.fileUrl(page.controller.selectedFile)
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                visible: !tintedPreviewArea.isAnimated
                                width: Math.min(tintedPreviewArea.width, tintedPreviewArea.height)
                                height: Math.min(tintedPreviewArea.width, tintedPreviewArea.height)
                            }
                            AnimatedImage {
                                anchors.centerIn: parent
                                source: tintedPreviewArea.isAnimated
                                    ? (page.controller.tintedPreviewUrl !== ""
                                        ? page.controller.tintedPreviewUrl
                                        : page.controller.fileUrl(page.controller.selectedFile))
                                    : ""
                                asynchronous: true
                                fillMode: Image.PreserveAspectFit
                                sourceSize: Qt.size(1280, 0)
                                playing: page.pageActive
                                width: Math.min(tintedPreviewArea.width, tintedPreviewArea.height)
                                height: Math.min(tintedPreviewArea.width, tintedPreviewArea.height)
                            }
                            // The tint pass runs off the UI thread; only this area spins because a queued apply just copies the file once the pass lands.
                            // The translucent chip keeps the spinner visible over the image.
                            Rectangle {
                                anchors.centerIn: parent
                                visible: page.controller.previewBusy
                                width: previewBusySpinner.width + Kirigami.Units.largeSpacing
                                height: previewBusySpinner.height + Kirigami.Units.largeSpacing
                                radius: Kirigami.Units.smallSpacing
                                color: Qt.rgba(Kirigami.Theme.backgroundColor.r,
                                               Kirigami.Theme.backgroundColor.g,
                                               Kirigami.Theme.backgroundColor.b, 0.8)
                                border.color: Kirigami.Theme.textColor
                                border.width: 0.5
                                BusyIndicator {
                                    id: previewBusySpinner
                                    anchors.centerIn: parent
                                    running: parent.visible
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
