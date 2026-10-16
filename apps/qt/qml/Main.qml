import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 1020
    height: 680
    minimumWidth: 760
    minimumHeight: 520
    title: "ClipForge Batch"
    color: theme.navy

    Theme {
        id: theme
    }

    property int totalDone: 0
    property int totalCount: 0

    Connections {
        target: bridge
        function onTotalProgress(completed, total) {
            root.totalDone = completed;
            root.totalCount = total;
        }
        function onBatchFinished(ok, message) {
            statusLine.text = (ok ? "Finished: " : "Stopped: ") + message;
        }
        function onBatchError(message) {
            statusLine.text = "Error: " + message;
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            spacing: 12
            Image {
                source: "qrc:/qt/qml/ClipForge/assets/clipforge-logo.svg"
                sourceSize.width: 40
                sourceSize.height: 40
                fillMode: Image.PreserveAspectFit
            }
            ColumnLayout {
                spacing: 0
                Label {
                    text: "ClipForge Batch"
                    font.pixelSize: 22
                    font.bold: true
                    color: theme.bone
                }
                Label {
                    text: "Offline batch audio toolkit"
                    font.pixelSize: 13
                    color: theme.slate
                }
            }
        }

        RowLayout {
            spacing: 12
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Label {
                    text: "Input directory"
                    font.pixelSize: 12
                    color: theme.slate
                }
                TextField {
                    id: inputDirField
                    objectName: "inputDirField"
                    Layout.fillWidth: true
                    implicitHeight: 38
                    leftPadding: 12
                    placeholderText: "D:\\Recordings"
                    color: theme.bone
                    placeholderTextColor: theme.slate
                    selectionColor: theme.cyan
                    background: Rectangle {
                        radius: theme.radiusControl
                        color: theme.panel
                        border.color: inputDirField.activeFocus ? theme.cyan : theme.border
                        border.width: inputDirField.activeFocus ? 2 : 1
                    }
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4
                Label {
                    text: "Output directory"
                    font.pixelSize: 12
                    color: theme.slate
                }
                TextField {
                    id: outputDirField
                    objectName: "outputDirField"
                    Layout.fillWidth: true
                    implicitHeight: 38
                    leftPadding: 12
                    placeholderText: "D:\\Processed"
                    color: theme.bone
                    placeholderTextColor: theme.slate
                    selectionColor: theme.cyan
                    background: Rectangle {
                        radius: theme.radiusControl
                        color: theme.panel
                        border.color: outputDirField.activeFocus ? theme.cyan : theme.border
                        border.width: outputDirField.activeFocus ? 2 : 1
                    }
                }
            }
            ColumnLayout {
                spacing: 4
                Label {
                    text: "Operation"
                    font.pixelSize: 12
                    color: theme.slate
                }
                ComboBox {
                    id: operationBox
                    implicitHeight: 38
                    model: ["normalize", "trim", "both"]
                    contentItem: Text {
                        text: operationBox.displayText
                        color: theme.bone
                        font.pixelSize: 14
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    background: Rectangle {
                        radius: theme.radiusControl
                        color: theme.panel
                        border.color: theme.border
                        border.width: 1
                    }
                    popup: Popup {
                        background: Rectangle {
                            color: theme.panel
                            border.color: theme.border
                            border.width: 1
                            radius: theme.radiusControl
                        }
                    }
                    delegate: ItemDelegate {
                        width: operationBox.width
                        contentItem: Text {
                            text: modelData
                            color: theme.bone
                            font.pixelSize: 14
                            elide: Text.ElideRight
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            color: highlighted ? theme.track : "transparent"
                        }
                    }
                }
            }
            ColumnLayout {
                spacing: 4
                Label {
                    text: "Options"
                    font.pixelSize: 12
                    color: theme.slate
                }
                RowLayout {
                    spacing: 12
                    CheckBox {
                        id: recursiveBox
                        text: "Recursive"
                        contentItem: Text {
                            text: recursiveBox.text
                            color: theme.bone
                            font.pixelSize: 14
                            leftPadding: recursiveBox.indicator.width + 8
                            verticalAlignment: Text.AlignVCenter
                        }
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: recursiveBox.leftPadding
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 6
                            color: recursiveBox.checked ? theme.cyan : "transparent"
                            border.color: recursiveBox.checked ? theme.cyan : theme.slate
                            border.width: 2
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 3
                                color: theme.navy
                                visible: recursiveBox.checked
                            }
                        }
                    }
                    CheckBox {
                        id: dryRunBox
                        checked: true
                        text: "Dry run"
                        contentItem: Text {
                            text: dryRunBox.text
                            color: theme.bone
                            font.pixelSize: 14
                            leftPadding: dryRunBox.indicator.width + 8
                            verticalAlignment: Text.AlignVCenter
                        }
                        indicator: Rectangle {
                            implicitWidth: 20
                            implicitHeight: 20
                            x: dryRunBox.leftPadding
                            anchors.verticalCenter: parent.verticalCenter
                            radius: 6
                            color: dryRunBox.checked ? theme.cyan : "transparent"
                            border.color: dryRunBox.checked ? theme.cyan : theme.slate
                            border.width: 2
                            Rectangle {
                                anchors.centerIn: parent
                                width: 10
                                height: 10
                                radius: 3
                                color: theme.navy
                                visible: dryRunBox.checked
                            }
                        }
                    }
                }
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            Rectangle {
                SplitView.preferredWidth: 600
                color: theme.panel
                radius: theme.radiusCard
                border.color: theme.border
                border.width: 1
                clip: true

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    RowLayout {
                        Label {
                            text: "Queue"
                            font.pixelSize: 15
                            font.bold: true
                            color: theme.bone
                        }
                        Label {
                            text: queueView.count + " files"
                            font.pixelSize: 12
                            font.family: theme.monoFont
                            color: theme.slate
                        }
                    }
                    ListView {
                        id: queueView
                        objectName: "queueView"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        reuseItems: true
                        spacing: 6
                        model: batchModel
                        delegate: ItemDelegate {
                            required property string fileName
                            required property string status
                            required property double progress
                            required property string errorMessage
                            width: queueView.width
                            contentItem: ColumnLayout {
                                spacing: 3
                                Label {
                                    Layout.fillWidth: true
                                    text: fileName
                                    elide: Text.ElideMiddle
                                    color: theme.bone
                                    font.pixelSize: 13
                                    font.family: theme.monoFont
                                }
                                RowLayout {
                                    spacing: 8
                                    ProgressBar {
                                        Layout.fillWidth: true
                                        from: 0
                                        to: 1
                                        value: progress
                                        background: Rectangle {
                                            implicitHeight: 8
                                            radius: 4
                                            color: theme.track
                                        }
                                        contentItem: Item {
                                            Rectangle {
                                                width: parent.width * progress
                                                height: parent.height
                                                radius: 4
                                                color: theme.cyan
                                            }
                                        }
                                    }
                                    Label {
                                        text: status
                                        font.pixelSize: 13
                                        color: theme.statusColor(status)
                                    }
                                }
                                Label {
                                    Layout.fillWidth: true
                                    text: errorMessage
                                    visible: errorMessage !== ""
                                    wrapMode: Text.WrapAnywhere
                                    font.pixelSize: 12
                                    color: theme.danger
                                }
                            }
                            background: Rectangle {
                                radius: 8
                                color: hovered ? theme.track : "transparent"
                            }
                        }
                        ScrollBar.vertical: ScrollBar {
                            policy: ScrollBar.AsNeeded
                            contentItem: Rectangle {
                                implicitWidth: 8
                                radius: 4
                                color: theme.border
                            }
                            background: Rectangle {
                                implicitWidth: 8
                                color: "transparent"
                            }
                        }
                        Label {
                            anchors.centerIn: parent
                            visible: queueView.count === 0
                            text: "No WAV files yet — pick an input folder and press Start."
                            wrapMode: Text.WordWrap
                            horizontalAlignment: Text.AlignHCenter
                            width: parent.width - 48
                            font.pixelSize: 14
                            color: theme.slate
                        }
                    }
                }
            }

            Rectangle {
                SplitView.preferredWidth: 320
                color: theme.panel
                radius: theme.radiusCard
                border.color: theme.border
                border.width: 1
                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 14
                    spacing: 8
                    Label {
                        text: "Inspector"
                        font.pixelSize: 15
                        font.bold: true
                        color: theme.bone
                    }
                    Label {
                        text: "File"
                        font.pixelSize: 12
                        color: theme.slate
                    }
                    Label {
                        Layout.fillWidth: true
                        text: queueView.currentItem ? queueView.currentItem.fileName : "No selection"
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: 13
                        color: theme.bone
                        font.family: theme.monoFont
                    }
                    Label {
                        text: "Status"
                        font.pixelSize: 12
                        color: theme.slate
                    }
                    Label {
                        text: queueView.currentItem ? queueView.currentItem.status : "—"
                        font.pixelSize: 14
                        font.bold: true
                        color: queueView.currentItem ? theme.statusColor(queueView.currentItem.status) : theme.slate
                    }
                }
            }
        }

        RowLayout {
            spacing: 12
            Label {
                text: totalDone + " / " + Math.max(totalCount, 0) + " files"
                font.pixelSize: 13
                font.family: theme.monoFont
                color: theme.slate
            }
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, root.totalCount)
                value: root.totalDone
                background: Rectangle {
                    implicitHeight: 10
                    radius: 5
                    color: theme.track
                }
                contentItem: Item {
                    Rectangle {
                        width: parent.width * (root.totalCount > 0 ? root.totalDone / root.totalCount : 0)
                        height: parent.height
                        radius: 5
                        color: theme.cyan
                    }
                }
            }
            Button {
                objectName: "startButton"
                text: "Start"
                font.pixelSize: 14
                font.bold: true
                contentItem: Text {
                    text: startButton.text
                    font: startButton.font
                    color: theme.navy
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 120
                    implicitHeight: 38
                    radius: theme.radiusControl
                    color: startButton.hovered ? theme.orangeEnd : theme.orange
                }
                onClicked: {
                    statusLine.text = "Running…";
                    bridge.startBatch(
                        inputDirField.text,
                        outputDirField.text,
                        operationBox.currentText,
                        -1.0, -45.0,
                        recursiveBox.checked,
                        dryRunBox.checked);
                }
            }
            Button {
                objectName: "cancelButton"
                text: "Cancel"
                font.pixelSize: 14
                contentItem: Text {
                    text: cancelButton.text
                    font: cancelButton.font
                    color: theme.bone
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle {
                    implicitWidth: 120
                    implicitHeight: 38
                    radius: theme.radiusControl
                    color: cancelButton.hovered ? theme.track : "transparent"
                    border.color: theme.slate
                    border.width: 1
                }
                onClicked: bridge.cancel()
            }
        }

        Label {
            id: statusLine
            objectName: "statusLine"
            Layout.fillWidth: true
            text: "Idle."
            font.pixelSize: 13
            color: theme.slate
        }
    }
}
