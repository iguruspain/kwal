import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.Page {
    id: page
    title: qsTr("Ulauncher")

    readonly property color pageBackground: app.enableTransparency
        ? Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
        : Kirigami.Theme.backgroundColor

    background: Rectangle {
        color: page.pageBackground
    }
    property AppController app

    readonly property UlauncherController controller: UlauncherController {}

    // The entry currently selected in the theme combobox.
    readonly property var selectedEntry: {
        const entries = page.controller.themeEntries
        const index = page.controller.selectedThemeIndex
        return (entries && index >= 0 && index < entries.length) ? entries[index] : null
    }

    // True when no palette section holds any editable color row.
    readonly property bool noColors: {
        const sections = page.controller.paletteSections
        if (!sections)
            return true
        for (let i = 0; i < sections.length; i++)
            if (sections[i].colors.length > 0)
                return false
        return true
    }

    Dialog {
        id: resultDialog
        title: qsTr("Ulauncher")
        modal: true
        visible: false
        standardButtons: Dialog.Ok
        property string message: ""
        contentItem: Label {
            text: resultDialog.message
            wrapMode: Text.WordWrap
        }
    }

    Dialog {
        id: newThemeDialog
        title: qsTr("Create New Theme")
        modal: true
        visible: false
        standardButtons: Dialog.Cancel | Dialog.Ok
        property string themeName: ""
        contentItem: TextField {
            id: newThemeNameField
            text: newThemeDialog.themeName
            onTextChanged: newThemeDialog.themeName = text
            placeholderText: qsTr("Theme name")
            onAccepted: {
                if (text.trim().length > 0)
                    newThemeDialog.accept()
            }
        }
        onAccepted: {
            const name = newThemeDialog.themeName.trim()
            if (name.length > 0)
                page.controller.createTheme(name)
        }
        onOpened: {
            newThemeDialog.themeName = ""
            newThemeNameField.forceActiveFocus()
        }
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

        // --- Left pane: settings ---
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
                        text: qsTr("Select a theme")
                        Layout.fillWidth: true
                    }
                    ToolButton {
                        icon.name: "edit-clear"
                        ToolTip.text: qsTr("Reset to current theme")
                        ToolTip.visible: hovered
                        onClicked: page.controller.clearSelection()
                    }
                }

                ComboBox {
                    id: themeCombo
                    Layout.fillWidth: true
                    model: page.controller.themeEntries
                    textRole: "name"
                    Component.onCompleted: themeCombo.currentIndex = page.controller.selectedThemeIndex
                    onActivated: page.controller.selectTheme(currentIndex)
                }

                Connections {
                    target: page.controller
                    function onSelectedThemeIndexChanged() {
                        if (themeCombo.currentIndex !== page.controller.selectedThemeIndex)
                            themeCombo.currentIndex = page.controller.selectedThemeIndex
                    }
                    function onThemeEntriesChanged() {
                        themeCombo.currentIndex = page.controller.selectedThemeIndex
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    Label {
                        text: qsTr("Window width")
                        Layout.fillWidth: true
                    }
                    SpinBox {
                        from: 400
                        to: 1200
                        stepSize: 5
                        value: page.controller.previewWindowWidth
                        onValueModified: page.controller.setPreviewWindowWidth(Math.round(value))
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                ScrollView {
                    id: paletteScroll
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    ColumnLayout {
                        Layout.fillWidth: true
                        width: paletteScroll.availableWidth
                        spacing: Kirigami.Units.largeSpacing

                        // paletteSections order is fixed by the controller:
                        // index 0 = Manifest Colors, index 1 = Theme Colors.
                        Label {
                            text: {
                                const s = page.controller.paletteSections
                                return (s && s.length > 0) ? s[0].title : ""
                            }
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Repeater {
                            model: {
                                const s = page.controller.paletteSections
                                return (s && s.length > 0) ? s[0].colors : []
                            }
                            delegate: ColorRow {
                                onColorAccepted: (hex) =>
                                    page.controller.setColorValue(modelData.key, hex)
                            }
                        }

                        Kirigami.Separator { Layout.fillWidth: true }

                        Label {
                            text: {
                                const s = page.controller.paletteSections
                                return (s && s.length > 1) ? s[1].title : ""
                            }
                            font.bold: true
                            Layout.fillWidth: true
                        }

                        Repeater {
                            model: {
                                const s = page.controller.paletteSections
                                return (s && s.length > 1) ? s[1].colors : []
                            }
                            delegate: ColorRow {
                                onColorAccepted: (hex) =>
                                    page.controller.setColorValue(modelData.key, hex)
                            }
                        }

                        Label {
                            visible: page.noColors
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
                        icon.name: "document-new"
                        ToolTip.text: qsTr("Create New Theme")
                        ToolTip.visible: hovered
                        onClicked: newThemeDialog.open()
                    }
                    ToolButton {
                        icon.name: "document-save"
                        ToolTip.text: qsTr("Save Theme")
                        ToolTip.visible: hovered
                        enabled: page.selectedEntry !== null && !page.selectedEntry.isTemplate
                        onClicked: page.controller.saveTheme()
                    }
                    ToolButton {
                        icon.name: "edit-delete"
                        ToolTip.text: qsTr("Delete Theme")
                        ToolTip.visible: hovered
                        enabled: page.selectedEntry !== null
                                 && !page.selectedEntry.isTemplate
                                 && !page.selectedEntry.isCurrent
                        onClicked: page.controller.deleteTheme()
                    }
                    ToolButton {
                        icon.name: "dialog-ok-apply"
                        ToolTip.text: qsTr("Apply to Ulauncher")
                        ToolTip.visible: hovered
                        enabled: page.selectedEntry !== null
                                 && !page.selectedEntry.isTemplate
                                 && !page.selectedEntry.isCurrent
                        onClicked: page.controller.applyToUlauncher()
                    }
                    ToolButton {
                        icon.name: "document-revert"
                        ToolTip.text: qsTr("Restore Backups")
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
            color: "transparent" //Qt.alpha(Kirigami.Theme.backgroundColor, 0.80)
            radius: Kirigami.Units.largeSpacing

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Kirigami.Units.gridUnit
                spacing: Kirigami.Units.gridUnit

                Label {
                    text: qsTr("Current Theme")
                    font.bold: true
                }
                Kirigami.Separator { Layout.fillWidth: true }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 350
                    UlauncherMockup {
                        id: currentMockup
                        anchors.fill: parent
                        html: page.controller.currentPreviewHtml
                        launcherWidth: page.controller.previewWindowWidth
                        onFitScaleChanged: page.controller.setPreviewScale(currentMockup.fitScale)
                    }
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No active theme")
                        color: Kirigami.Theme.disabledTextColor
                        visible: page.controller.currentPreviewHtml === ""
                    }
                }

                //Kirigami.Separator { Layout.fillWidth: true }

                Label {
                    text: qsTr("Live Preview")
                    font.bold: true
                }
                Kirigami.Separator { Layout.fillWidth: true }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: 350
                    UlauncherMockup {
                        id: liveMockup
                        anchors.fill: parent
                        html: page.controller.previewHtml
                        launcherWidth: page.controller.previewWindowWidth
                        onFitScaleChanged: page.controller.setPreviewScale(liveMockup.fitScale)
                    }
                    Label {
                        anchors.centerIn: parent
                        text: qsTr("No theme selected")
                        color: Kirigami.Theme.disabledTextColor
                        visible: page.controller.previewHtml === ""
                    }
                }
            }
        }
    }
}
