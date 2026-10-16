import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 960
    height: 640
    minimumWidth: 720
    minimumHeight: 480
    title: "ClipForge Batch"
    color: Theme.navy

    required property var bridge
    required property var batchModel

    property int totalDone: 0
    property int totalCount: 0

    Connections {
        target: root.bridge
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
        anchors.margins: 16
        spacing: 12

        Label {
            text: "ClipForge Batch"
            font.pixelSize: 22
            font.bold: true
            color: Theme.bone
        }

        RowLayout {
            spacing: 8
            TextField {
                id: inputDirField
                Layout.fillWidth: true
                placeholderText: "Input directory"
                color: Theme.bone
            }
            TextField {
                id: outputDirField
                Layout.fillWidth: true
                placeholderText: "Output directory"
                color: Theme.bone
            }
            ComboBox {
                id: operationBox
                model: ["normalize", "trim", "both"]
            }
            CheckBox {
                id: recursiveBox
                text: "Recursive"
            }
            CheckBox {
                id: dryRunBox
                text: "Dry run"
                checked: true
            }
        }

        SplitView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            orientation: Qt.Horizontal

            ListView {
                id: queueView
                SplitView.preferredWidth: 560
                clip: true
                model: root.batchModel
                delegate: ItemDelegate {
                    width: queueView.width
                    contentItem: ColumnLayout {
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: fileName
                            elide: Text.ElideMiddle
                            color: Theme.bone
                            font.family: Theme.monoFont
                        }
                        RowLayout {
                            ProgressBar {
                                Layout.fillWidth: true
                                from: 0
                                to: 1
                                value: progress
                            }
                            Label {
                                text: status
                                color: Theme.statusColor(status)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: errorMessage
                            visible: errorMessage !== ""
                            wrapMode: Text.WrapAnywhere
                            color: Theme.danger
                        }
                    }
                }
            }

            ColumnLayout {
                SplitView.preferredWidth: 340
                spacing: 6
                Label {
                    text: "Inspector"
                    font.bold: true
                    color: Theme.bone
                }
                Label {
                    Layout.fillWidth: true
                    text: queueView.currentItem ? queueView.currentItem.fileName : "No selection"
                    wrapMode: Text.WrapAnywhere
                    color: Theme.bone
                    font.family: Theme.monoFont
                }
                Label {
                    text: queueView.currentItem ? ("Status: " + queueView.currentItem.status) : ""
                    color: Theme.slate
                }
            }
        }

        RowLayout {
            spacing: 8
            ProgressBar {
                Layout.fillWidth: true
                from: 0
                to: Math.max(1, root.totalCount)
                value: root.totalDone
            }
            Button {
                text: "Start"
                onClicked: {
                    statusLine.text = "Running…";
                    root.bridge.startBatch(
                        inputDirField.text,
                        outputDirField.text,
                        operationBox.currentText,
                        -1.0, -45.0,
                        recursiveBox.checked,
                        dryRunBox.checked);
                }
            }
            Button {
                text: "Cancel"
                onClicked: root.bridge.cancel()
            }
        }

        Label {
            id: statusLine
            Layout.fillWidth: true
            text: "Idle."
            color: Theme.slate
        }
    }
}
