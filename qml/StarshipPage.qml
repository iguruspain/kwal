import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.Page {
    id: page
    title: qsTr("Starship")
    background: Rectangle {
        color: "transparent"
    }

    readonly property StarshipController controller: StarshipController {}

    Dialog {
        id: resultDialog
        title: qsTr("Starship")
        modal: true
        visible: false
        standardButtons: Dialog.Ok
        property string message: ""
        contentItem: Label {
            text: resultDialog.message
            wrapMode: Text.WordWrap
        }
    }

    ColorDialog {
        id: colorPicker
        onAccepted: {
            page.controller.draftColor = page.colorDialogHex()
        }
    }

    function colorDialogHex() {
        const c = colorPicker.selectedColor
        const a = Math.round(c.a * 255)
        let hex = a < 255
            ? a.toString(16).padStart(2, "0")
            : ""
        hex += Math.round(c.r * 255).toString(16).padStart(2, "0")
        hex += Math.round(c.g * 255).toString(16).padStart(2, "0")
        hex += Math.round(c.b * 255).toString(16).padStart(2, "0")
        return "#" + hex
    }

    function openColorDialog() {
        const current = page.controller.draftColor
        if (current !== "transparent" && current.startsWith("#")) {
            const hex = current.substring(1)
            colorPicker.selectedColor = hex.length === 8
                ? Qt.rgba(parseInt(hex.substring(2, 4), 16) / 255,
                           parseInt(hex.substring(4, 6), 16) / 255,
                           parseInt(hex.substring(6, 8), 16) / 255,
                           parseInt(hex.substring(0, 2), 16) / 255)
                : Qt.rgba(parseInt(hex.substring(0, 2), 16) / 255,
                           parseInt(hex.substring(2, 4), 16) / 255,
                           parseInt(hex.substring(4, 6), 16) / 255, 1)
        }
        colorPicker.open()
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
            page.controller.refresh()
    }

    // Hide the page-level header so the window tab bar is the only chrome.
    globalToolBarStyle: Kirigami.ApplicationHeaderStyle.None

    RowLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        // --- Left pane: controls ---
        Kirigami.Card {
            Layout.preferredWidth: 300
            Layout.minimumWidth: 300
            Layout.maximumWidth: 300
            Layout.fillHeight: true
            clip: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                Label {
                    text: qsTr("Settings")
                    font.bold: true
                }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Select a template")
                    }
                    ToolButton {
                        icon.name: "edit-clear"
                        ToolTip.text: qsTr("Reset to the current starship.toml")
                        ToolTip.visible: hovered
                        onClicked: page.controller.clearSelection()
                    }
                }

                ComboBox {
                    id: templateCombo
                    Layout.fillWidth: true
                    model: [qsTr("Current configuration")] + page.controller.templateNames
                    currentIndex: page.controller.templateIndex >= 0
                        ? page.controller.templateIndex + 1
                        : 0
                    onActivated: {
                        page.controller.templateIndex = currentIndex === 0
                            ? -1
                            : currentIndex - 1
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                Label {
                    text: qsTr("Palette")
                    visible: page.controller.paletteNames.length > 0
                }

                ComboBox {
                    id: paletteCombo
                    Layout.fillWidth: true
                    visible: page.controller.paletteNames.length > 0
                    model: page.controller.paletteNames
                    currentIndex: page.controller.draftPaletteIndex
                    onActivated: page.controller.draftPaletteIndex = currentIndex
                }

                Label {
                    text: qsTr("Color")
                    visible: page.controller.paletteNames.length > 0
                }

                ComboBox {
                    id: colorKeyCombo
                    Layout.fillWidth: true
                    visible: page.controller.paletteNames.length > 0
                    model: page.controller.draftPaletteIndex >= 0
                        && page.controller.draftPaletteIndex < page.controller.paletteKeys.length
                        ? page.controller.paletteKeys[page.controller.draftPaletteIndex]
                        : []
                    currentIndex: page.controller.draftColorIndex
                    onActivated: page.controller.draftColorIndex = currentIndex
                }

                RowLayout {
                    Layout.fillWidth: true
                    visible: page.controller.paletteNames.length > 0
                    Rectangle {
                        id: colorSwatch
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 1.5
                        Layout.preferredHeight: Kirigami.Units.gridUnit * 1.5
                        radius: Kirigami.Units.smallSpacing
                        color: page.controller.draftColor
                        border.color: Kirigami.Theme.disabledTextColor
                        border.width: 1
                    }
                    Item { Layout.fillWidth: true }
                    Button {
                        text: qsTr("Edit color")
                        onClicked: page.openColorDialog()
                    }
                }

                Item { Layout.fillHeight: true }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.largeSpacing

                    ToolButton {
                        icon.name: "dialog-ok-apply"
                        ToolTip.text: qsTr("Apply the edited configuration to starship.toml")
                        ToolTip.visible: hovered
                        onClicked: page.controller.applyConfig()
                    }
                    ToolButton {
                        icon.name: "document-revert"
                        ToolTip.text: qsTr("Restore starship.toml from the last backup")
                        ToolTip.visible: hovered
                        enabled: page.controller.hasBackup
                        onClicked: page.controller.restoreBackup()
                    }
                }
            }
        }

        // --- Right pane: preview ---
        Kirigami.Card {
            Layout.fillWidth: true
            Layout.fillHeight: true

            ColumnLayout {
                anchors.fill: parent
                spacing: Kirigami.Units.smallSpacing

                Label {
                    text: qsTr("Preview")
                    font.bold: true
                }

                Kirigami.Separator { Layout.fillWidth: true }

                Item {
                    id: previewArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    StarshipPreview {
                        anchors.fill: parent
                        html: page.controller.previewHtml
                    }
                    Label {
                        anchors.centerIn: previewArea
                        text: qsTr("No starship configuration found")
                        color: Kirigami.Theme.disabledTextColor
                        visible: page.controller.previewHtml === ""
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Configuration:")
                        color: Kirigami.Theme.disabledTextColor
                    }
                    Item { Layout.fillWidth: true }
                    Label {
                        text: page.controller.selectedTemplate === ""
                            ? page.controller.configPath
                            : page.controller.selectedTemplate
                        color: Kirigami.Theme.disabledTextColor
                    }
                }
            }
        }
    }
}
