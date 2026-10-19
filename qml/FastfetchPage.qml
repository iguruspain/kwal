import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.Page {
    id: page
    title: qsTr("Fastfetch")

    readonly property color pageBackground: app.enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
        : Kirigami.Theme.backgroundColor

    background: Rectangle {
        color: page.pageBackground
    }

    property AppController app

    readonly property FastfetchController controller: FastfetchController {}

    function shortPath(path) {
        const parts = String(path).split("/").filter(function (s) { return s.length > 0 })
        if (parts.length <= 2)
            return String(path)
        return "…" + "/" + parts[parts.length - 2] + "/" + parts[parts.length - 1]
    }

    Dialog {
        id: resultDialog
        title: qsTr("Fastfetch")
        modal: true
        visible: false
        standardButtons: Dialog.Ok
        property string message: ""
        contentItem: Label {
            text: resultDialog.message
            wrapMode: Text.WordWrap
        }
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
            resultDialog.message = message
            resultDialog.visible = true
        }
    }

    onVisibleChanged: {
        if (visible)
            page.controller.refreshConfigImage()
    }

    // Hide the page-level navigation header (back/forward + title) so the
    // window tab bar is the only chrome, matching WallpapersPage.
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    RowLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        // --- Left pane: controls ---
        // Fixed-width control panel: pin min/max/preferred so the Card's
        // implicit content width cannot inflate it (Card defaults to filling).
        Rectangle {
            id: leftPane
            Layout.preferredWidth: 300
            Layout.fillHeight: true
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
            radius: Kirigami.Units.largeSpacing
            border.color: Kirigami.Theme.highlightColor
            border.width: 1.5

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing
                spacing: Kirigami.Units.mediumSpacing

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
                        // The controller regenerates the tinted preview
                        // when the draft color changes.
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
                        onClicked: page.controller.applyOriginalImage()
                    }
                    ToolButton {
                        icon.name: "dialog-ok-apply"
                        ToolTip.text: qsTr("Apply the tinted image")
                        ToolTip.visible: hovered
                        enabled: page.controller.selectedFile !== ""
                            && page.controller.draftColor !== "transparent"
                        onClicked: page.controller.applyTintedImage()
                    }
                    ToolButton {
                        icon.name: "document-revert"
                        ToolTip.text: qsTr("Restore the fastfetch config from the last backup")
                        ToolTip.visible: hovered
                        enabled: page.controller.hasBackup
                        onClicked: page.controller.restoreBackup()
                    }
                }
            }
        }

        // --- Right pane: previews ---
        Rectangle {
            id: rightPane
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
            radius: Kirigami.Units.largeSpacing

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.mediumSpacing
                spacing: Kirigami.Units.mediumSpacing

                // 1. Image currently referenced by the fastfetch config.
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
                    Image {
                        anchors.centerIn: parent
                        source: page.controller.configImageUrl
                        asynchronous: true
                        cache: false
                        fillMode: Image.PreserveAspectFit
                        visible: page.controller.configImage !== ""
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

                // 2+3. Selected image and its tinted version, side by side.
                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 300
                    spacing: Kirigami.Units.largeSpacing
                    visible: page.controller.manuallySelected

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
                            Image {
                                anchors.centerIn: parent
                                source: page.controller.fileUrl(page.controller.selectedFile)
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectFit
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
                            Image {
                                anchors.centerIn: parent
                                source: page.controller.tintedPreviewUrl !== ""
                                    ? page.controller.tintedPreviewUrl
                                    : page.controller.fileUrl(page.controller.selectedFile)
                                asynchronous: true
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                width: Math.min(tintedPreviewArea.width, tintedPreviewArea.height)
                                height: Math.min(tintedPreviewArea.width, tintedPreviewArea.height)
                            }
                        }
                    }
                }
            }
        }
    }
}
