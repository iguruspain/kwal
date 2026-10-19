import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

pragma ComponentBehavior: Bound

Kirigami.Page {
    id: page
    title: qsTr("Ulauncher")
    background: Rectangle {
        color: "transparent"
    }

    readonly property UlauncherController controller: UlauncherController {}

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
                    text: qsTr("Theme")
                    font.bold: true
                }
                Label {
                    text: page.controller.themeName
                    color: Kirigami.Theme.disabledTextColor
                    elide: Text.ElideRight
                }

                Kirigami.Separator { Layout.fillWidth: true }

                Label {
                    text: qsTr("Color")
                }

                ComboBox {
                    id: colorCombo
                    Layout.fillWidth: true
                    model: page.controller.colorNames
                    currentIndex: page.controller.colorIndex
                    onActivated: page.controller.colorIndex = currentIndex
                }

                ColorDot {
                    color: page.controller.draftColor
                    onAccepted: (hex) => { page.controller.draftColor = hex }
                }

                Item { Layout.fillHeight: true }

                Kirigami.Separator { Layout.fillWidth: true }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignHCenter
                    spacing: Kirigami.Units.largeSpacing

                    ToolButton {
                        icon.name: "dialog-ok-apply"
                        ToolTip.text: qsTr("Apply the edited colors to the theme")
                        ToolTip.visible: hovered
                        onClicked: page.controller.applyConfig()
                    }
                    ToolButton {
                        icon.name: "document-revert"
                        ToolTip.text: qsTr("Restore the theme from the last backup")
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
                    UlauncherMockup {
                        anchors.fill: parent
                        html: page.controller.previewHtml
                    }
                }

                Kirigami.Separator { Layout.fillWidth: true }

                Label {
                    text: qsTr("Theme: %1").arg(page.controller.themeName)
                    color: Kirigami.Theme.disabledTextColor
                }
            }
        }
    }
}
