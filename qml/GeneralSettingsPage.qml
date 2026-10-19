import QtQuick
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KirigamiDelegates

Kirigami.ScrollablePage {
    id: page

    title: qsTr("General")

    required property AppController app

    contentItem: Item {
        ColumnLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.largeSpacing
            spacing: Kirigami.Units.smallSpacing

            KirigamiDelegates.SwitchSubtitleDelegate {
                id: transparencySwitch
                Layout.fillWidth: true
                text: qsTr("Enable transparency")
                subtitle: qsTr("Draw window backgrounds with reduced opacity")
                checked: page.app.enableTransparency
                onToggled: page.app.enableTransparency = checked
            }
        }
    }
}
