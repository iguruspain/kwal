import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KirigamiDelegates

Kirigami.ScrollablePage {
    id: page

    title: qsTr("General")

    required property AppController app

    // Setting labels exposed to the settings drawer search; keep in sync
    // with the controls below.
    property var searchKeys: [
        qsTr("Enable transparency"),
        qsTr("Transparency level"),
        qsTr("Default width"),
        qsTr("Default height")
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
            text: qsTr("Window")
            level: 3
        }

        KirigamiDelegates.SwitchSubtitleDelegate {
            id: transparencySwitch
            Layout.fillWidth: true
            text: qsTr("Enable transparency")
            subtitle: qsTr("Draw window backgrounds with reduced opacity")
            checked: page.app.enableTransparency
            onToggled: page.app.enableTransparency = checked
        }

        Kirigami.FormLayout {
            SpinBox {
                Kirigami.FormData.label: qsTr("Transparency level")
                from: 0
                to: 100
                stepSize: 5
                textFromValue: function(value) {
                    return value + "%"
                }
                value: page.app.transparencyLevel
                enabled: page.app.enableTransparency
                onValueModified: page.app.transparencyLevel = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Default width")
                from: 700
                to: 3840
                stepSize: 5
                value: page.app.defaultWindowWidth
                onValueModified: page.app.defaultWindowWidth = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Default height")
                from: 400
                to: 2160
                stepSize: 5
                value: page.app.defaultWindowHeight
                onValueModified: page.app.defaultWindowHeight = value
            }
        }
    }
}
