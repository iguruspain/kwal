import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    required property string toolName
    property string iconName: "application-x-executable"
    title: toolName

    contentItem: Item {
        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            icon.name: page.iconName
            text: qsTr("%1 is not available yet").arg(page.toolName)
            explanation: qsTr("Palette tinting for %1 is on the roadmap.").arg(page.toolName)
        }
    }
}
