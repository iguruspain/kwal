import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import org.kde.kirigami.delegates as KirigamiDelegates

Kirigami.ScrollablePage {
    id: page

    title: qsTr("Wallpapers")

    required property AppController app

    // Setting labels exposed to the settings drawer search; keep in sync
    // with the controls below.
    property var searchKeys: [
        qsTr("Show color dots (matugen)"),
        qsTr("Show color categories"),
        qsTr("Also apply to the lock screen"),
        qsTr("Resize videos"),
        qsTr("Target height"),
        qsTr("Frame rate"),
        qsTr("Folder"),
        qsTr("Generate resizes"),
        qsTr("Use transitions"),
        qsTr("Layer"),
        qsTr("Type"),
        qsTr("Advanced"),
        qsTr("Duration (s)"),
        qsTr("Step"),
        qsTr("Angle (°)"),
        qsTr("Wave width"),
        qsTr("Wave height"),
        qsTr("Position"),
        qsTr("Invert Y"),
        qsTr("Easing curve")
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
            text: qsTr("Colors")
            level: 3
        }

        KirigamiDelegates.SwitchSubtitleDelegate {
            Layout.fillWidth: true
            text: qsTr("Show color dots (matugen)")
            subtitle: qsTr("Show the matugen source colors of the selected wallpaper")
            enabled: page.app.matugenAvailable
            checked: page.app.matugenDotsEnabled
            onToggled: page.app.matugenDotsEnabled = checked
        }

        KirigamiDelegates.SwitchSubtitleDelegate {
            Layout.fillWidth: true
            text: qsTr("Show color categories")
            subtitle: qsTr("Show the color category chips in the wallpapers search")
            checked: page.app.colorCategoriesEnabled
            onToggled: page.app.colorCategoriesEnabled = checked
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            text: qsTr("Application")
            level: 3
        }

        KirigamiDelegates.SwitchSubtitleDelegate {
            Layout.fillWidth: true
            text: qsTr("Also apply to the lock screen")
            subtitle: qsTr("Write the applied wallpaper to the KScreenLock config too")
            checked: page.app.lockscreen
            onToggled: page.app.lockscreen = checked
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            text: qsTr("Transitions")
            level: 3
        }

        KirigamiDelegates.SwitchSubtitleDelegate {
            Layout.fillWidth: true
            text: qsTr("Use transitions")
            subtitle: qsTr("Animate the wallpaper change with an overlay effect")
            checked: page.app.transitionEnabled
            onToggled: page.app.transitionEnabled = checked
        }

        Kirigami.FormLayout {
            id: transitionForm
            Layout.fillWidth: true

            property string tType: page.app.transitionType
            property bool advancedOn: page.app.transitionEnabled && page.app.transitionAdvanced

            ComboBox {
                Kirigami.FormData.label: qsTr("Layer")
                model: page.app.transitionLayers
                currentIndex: model.indexOf(page.app.transitionLayer)
                enabled: page.app.transitionEnabled
                onActivated: page.app.transitionLayer = model[currentIndex]
            }

            ComboBox {
                Kirigami.FormData.label: qsTr("Type")
                model: page.app.transitionTypes
                currentIndex: model.indexOf(page.app.transitionType)
                enabled: page.app.transitionEnabled
                onActivated: page.app.transitionType = model[currentIndex]
            }

            Switch {
                Kirigami.FormData.label: qsTr("Advanced")
                checked: page.app.transitionAdvanced
                enabled: page.app.transitionEnabled
                onToggled: page.app.transitionAdvanced = checked
            }

            DoubleSpinBox {
                Kirigami.FormData.label: qsTr("Duration (s)")
                from: 0.1
                to: 10
                stepSize: 0.1
                decimals: 1
                value: page.app.transitionDuration
                visible: transitionForm.advancedOn && transitionForm.tType !== "simple"
                onValueModified: page.app.transitionDuration = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Frame rate")
                from: 0
                to: 240
                stepSize: 5
                textFromValue: function(value) {
                    return value === 0 ? qsTr("Original") : value
                }
                value: page.app.transitionFps
                visible: transitionForm.advancedOn && transitionForm.tType !== "simple"
                onValueModified: page.app.transitionFps = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Step")
                from: 1
                to: 255
                stepSize: 1
                value: page.app.transitionStep
                visible: transitionForm.advancedOn && transitionForm.tType === "simple"
                onValueModified: page.app.transitionStep = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Angle (°)")
                from: 0
                to: 360
                stepSize: 5
                value: page.app.transitionAngle
                visible: transitionForm.advancedOn && (transitionForm.tType === "wipe" || transitionForm.tType === "wave")
                onValueModified: page.app.transitionAngle = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Wave width")
                from: 1
                to: 500
                stepSize: 5
                value: page.app.transitionWaveWidth
                visible: transitionForm.advancedOn && transitionForm.tType === "wave"
                onValueModified: page.app.transitionWaveWidth = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Wave height")
                from: 0
                to: 500
                stepSize: 5
                value: page.app.transitionWaveHeight
                visible: transitionForm.advancedOn && transitionForm.tType === "wave"
                onValueModified: page.app.transitionWaveHeight = value
            }

            TextField {
                Kirigami.FormData.label: qsTr("Position")
                text: page.app.transitionPos
                placeholderText: qsTr("center, 0.5,0.5, 200,400, …")
                visible: transitionForm.advancedOn && (transitionForm.tType === "grow" || transitionForm.tType === "outer")
                onEditingFinished: page.app.transitionPos = text
            }

            Switch {
                Kirigami.FormData.label: qsTr("Invert Y")
                checked: page.app.transitionInvertY
                visible: transitionForm.advancedOn && (transitionForm.tType === "grow" || transitionForm.tType === "outer")
                onToggled: page.app.transitionInvertY = checked
            }

            TextField {
                Kirigami.FormData.label: qsTr("Easing curve")
                text: page.app.transitionBezier
                placeholderText: qsTr(".54,0,.34,.99")
                visible: transitionForm.advancedOn && transitionForm.tType !== "simple"
                onEditingFinished: page.app.transitionBezier = text
            }
        }

        Kirigami.Heading {
            Layout.fillWidth: true
            text: qsTr("Video resize")
            level: 3
        }

        KirigamiDelegates.SwitchSubtitleDelegate {
            Layout.fillWidth: true
            text: qsTr("Resize videos")
            subtitle: qsTr("Apply the scaled copy instead of the original video")
            checked: page.app.resizeEnabled
            onToggled: page.app.resizeEnabled = checked
        }

        Kirigami.FormLayout {
            SpinBox {
                Kirigami.FormData.label: qsTr("Target height")
                from: 480
                to: 4320
                stepSize: 120
                value: page.app.resizeHeight
                onValueModified: page.app.resizeHeight = value
            }

            SpinBox {
                Kirigami.FormData.label: qsTr("Frame rate")
                from: 0
                to: 240
                stepSize: 5
                textFromValue: function(value) {
                    return value === 0 ? qsTr("Original") : value
                }
                value: page.app.resizeFps
                onValueModified: page.app.resizeFps = value
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            ComboBox {
                id: folderCombo
                Layout.fillWidth: true
                model: page.app.videoFolders
                enabled: page.app.videoFolders.length > 0
            }

            Button {
                text: page.app.wallpapersInfo.resizing ? qsTr("Generating…") : qsTr("Generate resizes")
                enabled: page.app.videoFolders.length > 0 && !page.app.wallpapersInfo.resizing
                onClicked: {
                    const index = folderCombo.currentIndex
                    if (index >= 0 && index < folderCombo.model.length) {
                        page.app.generateVideoResizes(folderCombo.model[index])
                    }
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, page.app.wallpapersInfo.resizeTotal)
            value: page.app.wallpapersInfo.resizeCompleted
            visible: page.app.wallpapersInfo.resizing
        }
    }
}
