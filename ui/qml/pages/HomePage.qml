import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var presenter
    required property var runtime
    readonly property bool narrowLayout: width < 680

    readonly property var logLevels: ["trace", "debug", "info", "warn", "error", "critical", "off"]

    function refreshEndpointFields() {
        hostField.text = runtime.selectedMode === 0 ? runtime.serverHost : runtime.clientHost
        portField.text = String(runtime.selectedMode === 0 ? runtime.serverPort : runtime.clientPort)
    }

    Component.onCompleted: refreshEndpointFields()

    Connections {
        target: root.runtime
        function onSelectedModeChanged() { root.refreshEndpointFields() }
        function onServerHostChanged() { if (root.runtime.selectedMode === 0) root.refreshEndpointFields() }
        function onServerPortChanged() { if (root.runtime.selectedMode === 0) root.refreshEndpointFields() }
        function onClientHostChanged() { if (root.runtime.selectedMode === 1) root.refreshEndpointFields() }
        function onClientPortChanged() { if (root.runtime.selectedMode === 1) root.refreshEndpointFields() }
    }

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
                    text: "配置本机角色与网络参数，然后启动键鼠同步。"
                    color: "#727984"
                    font.pixelSize: 13
                }
            }
            Rectangle {
                Layout.preferredWidth: 9
                Layout.preferredHeight: 9
                radius: 5
                color: root.runtime.running ? "#2f9e62" : "#a1a7b0"
            }
            Text {
                text: root.runtime.statusTitle
                color: "#555d67"
                font.pixelSize: 12
            }
        }

        Components.SectionCard {
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width
                spacing: 0

                Text {
                    text: "运行设置"
                    color: "#303640"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.bottomMargin: 13
                }

                GridLayout {
                    Layout.fillWidth: true
                    columns: 3
                    columnSpacing: 12
                    rowSpacing: 9

                    Text {
                        text: "运行模式"
                        color: "#626a75"
                        font.pixelSize: 12
                        Layout.preferredWidth: 96
                    }
                    RowLayout {
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                        spacing: 22

                        RadioButton {
                            text: "服务端（共享本机键鼠）"
                            checked: root.runtime.selectedMode === 0
                            enabled: !root.runtime.active
                            onClicked: root.runtime.selectedMode = 0
                            palette.windowText: "#303640"
                            font.pixelSize: 12
                        }
                        RadioButton {
                            text: "客户端（接收远端键鼠）"
                            checked: root.runtime.selectedMode === 1
                            enabled: !root.runtime.active
                            onClicked: root.runtime.selectedMode = 1
                            palette.windowText: "#303640"
                            font.pixelSize: 12
                        }
                        Item { Layout.fillWidth: true }
                    }

                    Text {
                        text: root.runtime.selectedMode === 0 ? "监听地址" : "服务端地址"
                        color: "#626a75"
                        font.pixelSize: 12
                    }
                    Components.MksTextField {
                        id: hostField
                        enabled: !root.runtime.active
                        placeholderText: root.runtime.selectedMode === 0 ? "0.0.0.0" : "192.168.1.10"
                        Layout.fillWidth: true
                        onEditingFinished: {
                            if (root.runtime.selectedMode === 0)
                                root.runtime.serverHost = text
                            else
                                root.runtime.clientHost = text
                            root.refreshEndpointFields()
                        }
                    }
                    RowLayout {
                        spacing: 7
                        Text { text: "端口"; color: "#626a75"; font.pixelSize: 12 }
                        Components.MksTextField {
                            id: portField
                            enabled: !root.runtime.active
                            inputMethodHints: Qt.ImhDigitsOnly
                            validator: IntValidator { bottom: 1; top: 65535 }
                            Layout.preferredWidth: 92
                            onEditingFinished: {
                                if (acceptableInput) {
                                    if (root.runtime.selectedMode === 0)
                                        root.runtime.serverPort = Number(text)
                                    else
                                        root.runtime.clientPort = Number(text)
                                }
                                root.refreshEndpointFields()
                            }
                        }
                    }

                    Text {
                        text: "日志等级"
                        color: "#626a75"
                        font.pixelSize: 12
                    }
                    ComboBox {
                        id: logLevelBox
                        model: root.logLevels
                        currentIndex: Math.max(0, root.logLevels.indexOf(root.runtime.logLevel))
                        enabled: !root.runtime.active
                        implicitHeight: 34
                        Layout.preferredWidth: 160
                        onActivated: root.runtime.logLevel = currentText
                        palette.text: "#303640"
                        palette.buttonText: "#303640"
                        palette.base: "#ffffff"
                        palette.button: "#ffffff"
                        palette.highlight: "#356ae6"
                        palette.highlightedText: "#ffffff"
                        background: Rectangle {
                            radius: 3
                            color: logLevelBox.enabled ? "#ffffff" : "#f1f2f4"
                            border.width: logLevelBox.activeFocus ? 2 : 1
                            border.color: logLevelBox.activeFocus ? "#356ae6" : "#cbd0d7"
                        }
                    }
                    Text {
                        text: "控制核心运行日志的详细程度"
                        color: "#8a919b"
                        font.pixelSize: 11
                        Layout.fillWidth: true
                    }

                    Text {
                        text: "配置文件"
                        color: "#626a75"
                        font.pixelSize: 12
                    }
                    Text {
                        text: root.presenter.configPath.length === 0
                            ? "尚未保存，请先到“配置文件”页面保存"
                            : root.presenter.configPath
                        color: root.presenter.configPath.length === 0 ? "#a06f13" : "#4f5661"
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                        Layout.columnSpan: 2
                        Layout.fillWidth: true
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 1
                    color: "#e5e7ea"
                    Layout.topMargin: 14
                    Layout.bottomMargin: 12
                }
                Text {
                    text: root.runtime.selectedMode === 0
                        ? "服务端监听指定地址，客户端连接后鼠标可跨越到已配置的远端屏幕。"
                        : "客户端连接指定服务端并接收键盘、鼠标事件；本机不会捕获输入。"
                    color: "#767d87"
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
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
                            hostField.focus = false
                            portField.focus = false
                            if (root.runtime.active)
                                root.runtime.stop()
                            else
                                root.runtime.start(root.presenter.configPath)
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
