import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page

    required property string sectionName
    required property string iconName

    // Setting labels exposed to the settings drawer search; sections without
    // settings yet leave this empty.
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
