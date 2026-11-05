import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

ToolPage {
    id: page
    title: qsTr("Starship")
    resultTitle: qsTr("Starship")

    readonly property StarshipController controller: StarshipController {}

    // Rows of the palette being edited (draft): one {key, display} map per
    // color key, in the order stored in the config file.
    readonly property var paletteRows: {
        const index = page.controller.draftPaletteIndex
        const keys = page.controller.paletteKeys
        const values = page.controller.paletteValues
        if (!keys || index < 0 || index >= keys.length)
            return []
        const rows = []
        for (let i = 0; i < keys[index].length; i++)
            rows.push({
                key: keys[index][i],
                display: values && index < values.length && i < values[index].length
                    ? values[index][i] : ""
            })
        return rows
    }

    // Status line under "Status:", mirroring the old app's mode labels.
    readonly property string statusText: {
        const c = page.controller
        if (c.selectedTemplate !== "")
            return qsTr("Template Mode: %1").arg(c.selectedTemplate)
        if (c.hasConfig)
            return qsTr("Local Config Mode: %1").arg(c.configPath)
        return qsTr("No current starship.toml")
    }

    // The palette section starts expanded, as in the old app.
    property bool paletteExpanded: true

    // Scale that fits the natural-size terminal (800x200) into the current
    // config preview area; the same formula the original app used.
    readonly property real previewScale: {
        if (currentPreviewArea.width <= 0 || currentPreviewArea.height <= 0)
            return 0.9
        return Math.min(currentPreviewArea.width / 800,
                        currentPreviewArea.height / 200) * 0.9
    }

    onPreviewScaleChanged: page.controller.setPreviewScale(page.previewScale)
    Component.onCompleted: page.controller.setPreviewScale(page.previewScale)

    Connections {
        target: page.controller
        function onOperationResult(success, message) {
            page.showResult(message)
        }
    }

    onVisibleChanged: {
        if (visible)
            page.controller.refresh()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        // --- Left pane: settings ---
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

                Label {
                    text: qsTr("Settings")
                    font.bold: true
                }
                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Select a template")
                        Layout.fillWidth: true
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
                    model: [page.controller.hasConfig
                                ? page.controller.configPath.split("/").pop()
                                : qsTr("No starship.toml")]
                            .concat(page.controller.templateNames)
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

                ScrollView {
                    id: paletteScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    visible: page.controller.paletteNames.length > 0

                    ColumnLayout {
                        Layout.fillWidth: true
                        width: paletteScroll.availableWidth
                        spacing: Kirigami.Units.largeSpacing

                        RowLayout {
                            Layout.fillWidth: true
                            ToolButton {
                                icon.name: page.paletteExpanded ? "arrow-down" : "arrow-right"
                                onClicked: page.paletteExpanded = !page.paletteExpanded
                                ToolTip.text: page.paletteExpanded
                                    ? qsTr("Collapse palette")
                                    : qsTr("Expand palette")
                                ToolTip.visible: hovered
                            }
                            Label {
                                text: page.controller.draftPaletteIndex >= 0
                                        && page.controller.draftPaletteIndex < page.controller.paletteNames.length
                                    ? page.controller.paletteNames[page.controller.draftPaletteIndex]
                                    : qsTr("No palette")
                                font.bold: true
                            }
                            Item { Layout.fillWidth: true }
                        }
                        Kirigami.Separator { Layout.fillWidth: true }
                        Item { Layout.fillWidth: true }
                        ColumnLayout {
                            Layout.fillWidth: true
                            visible: page.paletteExpanded
                            spacing: Kirigami.Units.smallSpacing

                            Repeater {
                                model: page.paletteRows
                                delegate: ColorRow {
                                    onColorAccepted: (hex) =>
                                        page.controller.setColorValue(modelData.key, hex)
                                }
                            }
                        }
                        Label {
                            visible: page.paletteRows.length === 0
                            text: qsTr("No colors found")
                            font.italic: true
                            Layout.alignment: Qt.AlignHCenter
                        }
                    }
                }

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

        // --- Right pane: previews ---
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
                    text: qsTr("Current config:")
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    text: page.controller.hasConfig
                        ? page.controller.configPath
                        : qsTr("No starship.toml found")
                    font.italic: true
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.WrapAnywhere
                }

                Item {
                    id: currentPreviewArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 200
                    StarshipPreview {
                        anchors.fill: parent
                        html: page.controller.currentPreviewHtml
                    }
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No starship configuration found")
                        color: Kirigami.Theme.disabledTextColor
                        visible: page.controller.currentPreviewHtml === ""
                    }
                }

                Label {
                    text: qsTr("Status:")
                    font.bold: true
                }
                Label {
                    Layout.fillWidth: true
                    text: page.statusText
                    font.italic: true
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideMiddle
                }

                Item {
                    id: livePreviewArea
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 200
                    StarshipPreview {
                        anchors.fill: parent
                        html: page.controller.previewHtml
                    }
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No preview available")
                        color: Kirigami.Theme.disabledTextColor
                        visible: page.controller.previewHtml === ""
                    }
                }
            }
        }
    }
}
