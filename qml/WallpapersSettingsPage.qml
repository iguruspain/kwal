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
        qsTr("Easing curve"),
        qsTr("API endpoint"),
        qsTr("Model"),
        qsTr("System prompt"),
        qsTr("Tag wallpapers")
    ]

    // The page is hosted by the settings window; notify there so the
    // message lands on top of it.
    function notify(message) {
        // qmllint disable missing-property
        const win = page.parent
        if (win && win.showPassiveNotification) {
            win.showPassiveNotification(message, "long")
        }
        // qmllint enable missing-property
    }

    Connections {
        target: page.app.aiTagger
        function onTaggingFinished(succeeded, failed) {
            let message
            if (failed > 0) {
                message = qsTr("Tagging finished: %1 succeeded, %2 failed").arg(succeeded).arg(failed)
            } else if (succeeded === 1) {
                message = qsTr("Tagging finished: 1 wallpaper tagged")
            } else {
                message = qsTr("Tagging finished: %1 wallpapers tagged").arg(succeeded)
            }
            page.notify(message)
        }
        function onError(message) {
            page.notify(message)
        }
    }

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
            Layout.topMargin: Kirigami.Units.largeSpacing
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
            Layout.topMargin: Kirigami.Units.largeSpacing
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
                activeFocusOnTab: true
                Kirigami.FormData.label: qsTr("Duration (s)")
                from: 0.1
                to: 10
                stepSize: 0.1
                decimals: 1
                value: page.app.transitionDuration
                visible: transitionForm.advancedOn && transitionForm.tType !== "simple"
                onValueModified: page.app.transitionDuration = value
            }

            FpsComboBox {
                Kirigami.FormData.label: qsTr("Frame rate")
                fps: page.app.transitionFps
                visible: transitionForm.advancedOn && transitionForm.tType !== "simple"
                onFpsChanged: page.app.transitionFps = fps
            }

            SpinBox {
                activeFocusOnTab: true
                Kirigami.FormData.label: qsTr("Step")
                from: 1
                to: 255
                stepSize: 1
                value: page.app.transitionStep
                visible: transitionForm.advancedOn && transitionForm.tType === "simple"
                onValueModified: page.app.transitionStep = value
            }

            SpinBox {
                activeFocusOnTab: true
                Kirigami.FormData.label: qsTr("Angle (°)")
                from: 0
                to: 360
                stepSize: 5
                value: page.app.transitionAngle
                visible: transitionForm.advancedOn && (transitionForm.tType === "wipe" || transitionForm.tType === "wave")
                onValueModified: page.app.transitionAngle = value
            }

            SpinBox {
                activeFocusOnTab: true
                Kirigami.FormData.label: qsTr("Wave width")
                from: 1
                to: 500
                stepSize: 5
                value: page.app.transitionWaveWidth
                visible: transitionForm.advancedOn && transitionForm.tType === "wave"
                onValueModified: page.app.transitionWaveWidth = value
            }

            SpinBox {
                activeFocusOnTab: true
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
            Layout.topMargin: Kirigami.Units.largeSpacing
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
            ComboBox {
                id: resizeHeightCombo
                Kirigami.FormData.label: qsTr("Target height")
                editable: true
                model: ["720p", "1080p", "1440p", "2160p"]

                // The typed text lives in editText; a dropdown selection
                // leaves the field empty and currentText holds the item.
                function fieldText() {
                    return resizeHeightCombo.editText === ""
                        ? resizeHeightCombo.currentText
                        : resizeHeightCombo.editText
                }

                function parseText(t) {
                    const n = parseInt(t.trim().replace(/p$/i, ""), 10)
                    return isNaN(n) ? -1 : n
                }

                // The preset list is a convenience, not a limit: a stored
                // height outside it shows up as free text and the C++
                // setter clamps typed values.
                function sync() {
                    const v = page.app.resizeHeight
                    const i = model.indexOf(v + "p")
                    if (i >= 0) {
                        resizeHeightCombo.currentIndex = i
                    } else {
                        resizeHeightCombo.currentIndex = -1
                        resizeHeightCombo.editText = v + "p"
                    }
                }

                Component.onCompleted: sync()
                Connections {
                    target: page.app
                    function onResizeHeightChanged() { resizeHeightCombo.sync() }
                }

                onActivated: {
                    const v = parseText(fieldText())
                    if (v > 0) {
                        page.app.resizeHeight = v
                    } else {
                        sync()
                    }
                }

                onAccepted: {
                    const v = parseText(fieldText())
                    if (v > 0) {
                        page.app.resizeHeight = v
                    } else {
                        sync()
                    }
                }
            }

            FpsComboBox {
                Kirigami.FormData.label: qsTr("Frame rate")
                fps: page.app.resizeFps
                onFpsChanged: page.app.resizeFps = fps
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

        Kirigami.Heading {
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
            text: qsTr("AI tagging")
            level: 3
        }

        Kirigami.FormLayout {
            Layout.fillWidth: true

            TextField {
                Kirigami.FormData.label: qsTr("API endpoint")
                text: page.app.aiTagger.endpoint
                placeholderText: "http://localhost:18080"
                onEditingFinished: {
                    page.app.aiTagger.endpoint = text
                    if (text !== "" && page.app.aiTagger.models.length === 0) {
                        page.app.aiTagger.fetchModels()
                    }
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Model")
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                ComboBox {
                    id: modelCombo
                    Layout.fillWidth: true
                    model: page.app.aiTagger.models
                    displayText: page.app.aiTagger.model
                    enabled: page.app.aiTagger.endpoint !== ""
                    ToolTip.text: page.app.aiTagger.modelsError
                    ToolTip.visible: hovered && page.app.aiTagger.modelsError !== ""
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onActivated: (index) => page.app.aiTagger.model = modelCombo.textAt(index)
                }

                ToolButton {
                    icon.name: page.app.aiTagger.modelsLoading ? "hourglass" : "view-refresh"
                    enabled: page.app.aiTagger.endpoint !== "" && !page.app.aiTagger.modelsLoading
                    Accessible.name: qsTr("Fetch models")
                    ToolTip.text: qsTr("Fetch the available models from the API")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: page.app.aiTagger.fetchModels()
                }
            }

            ScrollView {
                id: promptView
                Kirigami.FormData.label: qsTr("System prompt")
                Layout.fillWidth: true
                Layout.preferredHeight: Kirigami.Units.gridUnit * 6
                implicitHeight: Kirigami.Units.gridUnit * 6

                TextArea {
                    id: promptArea
                    Layout.fillWidth: true
                    width: promptView.width
                    
                    text: page.app.aiTagger.systemPrompt !== ""
                        ? page.app.aiTagger.systemPrompt
                        : page.app.aiTagger.defaultSystemPrompt
                    
                    wrapMode: TextArea.Wrap
                    onEditingFinished: page.app.aiTagger.systemPrompt = text
                }
            }

            RowLayout {
                Kirigami.FormData.label: qsTr("Folder")
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                ComboBox {
                    id: aiFolderCombo
                    Layout.fillWidth: true
                    model: page.app.folders
                    currentIndex: page.app.folders.indexOf(page.app.aiTagger.folder)
                    enabled: page.app.folders.length > 0
                    onActivated: (index) => page.app.aiTagger.folder = aiFolderCombo.textAt(index)
                }

                ToolButton {
                    icon.name: page.app.aiTagger.tagging ? "hourglass" : "tag-new"
                    display: AbstractButton.IconOnly
                    Accessible.name: qsTr("Tag wallpapers")
                    ToolTip.text: qsTr("Tag every wallpaper in the folder with AI")
                    ToolTip.visible: hovered
                    ToolTip.delay: Kirigami.Units.toolTipDelay
                    enabled: !page.app.aiTagger.tagging
                        && page.app.aiTagger.endpoint !== ""
                        && page.app.aiTagger.model !== ""
                        && page.app.folders.length > 0
                    onClicked: page.app.aiTagger.tagFolder(page.app.aiTagger.folder)
                }
            }
        }

        ProgressBar {
            Layout.fillWidth: true
            from: 0
            to: Math.max(1, page.app.aiTagger.tagTotal)
            value: page.app.aiTagger.tagCompleted
            visible: page.app.aiTagger.tagging
        }
    }
}
