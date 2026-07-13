import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var runtime
    readonly property bool narrowLayout: width < 680

    signal configureRequested()

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Text {
                    text: "运行"
                    color: "#20242c"
                    font.pixelSize: root.narrowLayout ? 20 : 23
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "启动或停止键鼠同步，并查看核心运行日志。"
                    color: "#727984"
                    font.pixelSize: 13
                }
            }
            Components.MksButton {
                text: "启动配置…"
                secondary: true
                enabled: !root.runtime.active
                implicitWidth: 92
                implicitHeight: 30
                onClicked: root.configureRequested()
            }
        }

        Components.SectionCard {
            Layout.fillWidth: true
            Layout.fillHeight: true
            contentMargins: 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 12
                    Layout.topMargin: 10
                    Layout.bottomMargin: 10
                    spacing: 10

                    Rectangle {
                        Layout.preferredWidth: 9
                        Layout.preferredHeight: 9
                        radius: 5
                        color: root.runtime.running ? "#2f9e62"
                            : (root.runtime.active ? "#d29a28" : "#a1a7b0")
                    }
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 1
                        Text {
                            text: root.runtime.statusTitle
                            color: "#303640"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                        }
                        Text {
                            text: root.runtime.statusDetail
                            color: "#7b828c"
                            font.pixelSize: 11
                        }
                    }
                    Components.MksButton {
                        text: "清空日志"
                        secondary: true
                        enabled: root.runtime.logText.length > 0
                        implicitWidth: 78
                        implicitHeight: 30
                        onClicked: root.runtime.clearLog()
                    }
                    Components.MksButton {
                        text: root.runtime.active ? "停止" : "启动"
                        destructive: root.runtime.active
                        implicitWidth: 82
                        implicitHeight: 32
                        onClicked: {
                            if (root.runtime.active)
                                root.runtime.stop()
                            else
                                root.runtime.start()
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e1e4e8" }

                ScrollView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true

                    TextArea {
                        id: logArea
                        text: root.runtime.logText.length === 0
                            ? "运行日志将在这里显示。"
                            : root.runtime.logText
                        readOnly: true
                        selectByMouse: true
                        wrapMode: TextEdit.WrapAnywhere
                        color: root.runtime.logText.length === 0 ? "#9aa0a9" : "#4c535d"
                        font.family: "monospace"
                        font.pixelSize: 11
                        leftPadding: 14
                        rightPadding: 14
                        topPadding: 10
                        bottomPadding: 10
                        background: Rectangle { color: "#fafbfc" }
                        onTextChanged: cursorPosition = length
                    }
                }
            }
        }
    }
}
