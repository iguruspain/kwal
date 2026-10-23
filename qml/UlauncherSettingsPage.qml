import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page

    title: qsTr("Ulauncher")

    required property UlauncherController controller

    // Setting labels exposed to the settings drawer search; keep in sync
    // with the controls below.
    property var searchKeys: [
        qsTr("Ulauncher width"),
        qsTr("Input text"),
        qsTr("Sample rows")
    ]

    // Direct child of the ScrollablePage (no contentItem override): the page
    // anchors left/right to its scroll area; the top anchor keeps the content
    // flush with the top so the page scrolls when it overflows.
    ColumnLayout {
        anchors {
            top: parent.top
            left: parent.left
            right: parent.right
            margins: Kirigami.Units.largeSpacing
        }
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Heading {
            Layout.fillWidth: true
            text: qsTr("Preview")
            level: 3
        }

        Kirigami.FormLayout {
            SpinBox {
                Kirigami.FormData.label: qsTr("Ulauncher width")
                from: 400
                to: 1200
                stepSize: 10
                value: page.controller.previewWindowWidth
                onValueModified: page.controller.setPreviewWindowWidth(Math.round(value))
            }

            TextField {
                Kirigami.FormData.label: qsTr("Input text")
                text: page.controller.previewQuery
                placeholderText: qsTr("Text typed in the launcher input")
                onTextChanged: page.controller.setPreviewQuery(text)
            }
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            text: qsTr("Sample rows")
            level: 3
        }

        Label {
            text: qsTr("Items shown in the launcher preview. The input text's characters are highlighted in each name.")
            wrapMode: Text.WordWrap
            opacity: 0.7
        }

        Repeater {
            model: page.controller.sampleRows

            delegate: RowLayout {
                id: row
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing
                property int rowId: index

                function submit() {
                    page.controller.setSampleRow(row.rowId, {
                        "icon": iconField.text,
                        "name": nameField.text,
                        "shortcut": shortcutField.text,
                        "selected": selectedCheck.checked
                    })
                }

                TextField {
                    id: iconField
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 8
                    text: modelData.icon
                    placeholderText: qsTr("Icon")
                    onTextChanged: row.submit()
                }

                TextField {
                    id: nameField
                    Layout.fillWidth: true
                    text: modelData.name
                    placeholderText: qsTr("Name")
                    onTextChanged: row.submit()
                }

                TextField {
                    id: shortcutField
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 7
                    text: modelData.shortcut
                    placeholderText: qsTr("Shortcut")
                    onTextChanged: row.submit()
                }

                CheckBox {
                    id: selectedCheck
                    checked: modelData.selected
                    ToolTip.text: qsTr("Selected row")
                    ToolTip.visible: hovered
                    onToggled: row.submit()
                }

                ToolButton {
                    icon.name: "edit-delete"
                    ToolTip.text: qsTr("Remove row")
                    ToolTip.visible: hovered
                    onClicked: page.controller.removeSampleRow(row.rowId)
                }
            }
        }

        ToolButton {
            icon.name: "list-add"
            text: qsTr("Add row")
            Layout.alignment: Qt.AlignLeft
            onClicked: page.controller.addSampleRow()
        }
    }
}
