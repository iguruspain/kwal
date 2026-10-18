import QtQuick
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    required property string toolName
    title: toolName

    contentItem: Item {
        Kirigami.PlaceholderMessage {
            anchors.centerIn: parent
            icon.name: "application-x-executable"
            text: qsTr("%1 is not available yet").arg(page.toolName)
            explanation: qsTr("Palette tinting for %1 is on the roadmap.").arg(page.toolName)
        }
    }
}
