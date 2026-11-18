import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    required property string sectionName
    required property string iconName

    // Required by the settings drawer search; empty for sections without settings.
    property var searchKeys: []

    title: sectionName

    contentItem: Item {
        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            icon.name: page.iconName
            text: qsTr("%1 settings are not available yet").arg(page.sectionName)
            explanation: qsTr("This section is still under construction.")
        }
    }
}
