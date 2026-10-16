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
        anchors.margins: 16
        spacing: 12

        Label {
            text: "ClipForge Batch"
            font.pixelSize: 22
            font.bold: true
            color: theme.bone
        }

        RowLayout {
            spacing: 8
            TextField {
                id: inputDirField
                objectName: "inputDirField"
                Layout.fillWidth: true
                placeholderText: "Input directory"
                color: theme.bone
            }
            TextField {
                id: outputDirField
                objectName: "outputDirField"
                Layout.fillWidth: true
                placeholderText: "Output directory"
                color: theme.bone
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
                objectName: "queueView"
                SplitView.preferredWidth: 560
                clip: true
                reuseItems: true
                model: batchModel
                delegate: ItemDelegate {
                    required property string fileName
                    required property string status
                    required property double progress
                    required property string errorMessage
                    width: queueView.width
                    contentItem: ColumnLayout {
                        spacing: 2
                        Label {
                            Layout.fillWidth: true
                            text: fileName
                            elide: Text.ElideMiddle
                            color: theme.bone
                            font.family: theme.monoFont
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
                                color: theme.statusColor(status)
                            }
                        }
                        Label {
                            Layout.fillWidth: true
                            text: errorMessage
                            visible: errorMessage !== ""
                            wrapMode: Text.WrapAnywhere
                            color: theme.danger
                        }
                    }
                }

                Label {
                    anchors.centerIn: parent
                    visible: queueView.count === 0
                    text: "No WAV files yet — pick an input folder and press Start."
                    wrapMode: Text.WordWrap
                    horizontalAlignment: Text.AlignHCenter
                    width: parent.width - 32
                    color: theme.slate
                }
            }

            ColumnLayout {
                SplitView.preferredWidth: 340
                spacing: 6
                Label {
                    text: "Inspector"
                    font.bold: true
                    color: theme.bone
                }
                Label {
                    Layout.fillWidth: true
                    text: queueView.currentItem ? queueView.currentItem.fileName : "No selection"
                    wrapMode: Text.WrapAnywhere
                    color: theme.bone
                    font.family: theme.monoFont
                }
                Label {
                    text: queueView.currentItem ? ("Status: " + queueView.currentItem.status) : ""
                    color: theme.slate
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
                objectName: "startButton"
                text: "Start"
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
                onClicked: bridge.cancel()
            }
        }

        Label {
            id: statusLine
            objectName: "statusLine"
            Layout.fillWidth: true
            text: "Idle."
            color: theme.slate
        }
    }
}
