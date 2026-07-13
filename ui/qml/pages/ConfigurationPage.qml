import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var presenter
    required property var runtime
    readonly property bool narrowLayout: width < 680
    readonly property var logLevels: ["trace", "info", "warn", "error", "critical", "off"]

    function refreshEndpointField() {
        endpointField.text = runtime.endpoint
    }

    Component.onCompleted: refreshEndpointField()

    Connections {
        target: root.runtime
        function onEndpointChanged() { root.refreshEndpointField() }
    }

    FileDialog {
        id: startupImportDialog
        title: "导入命令行启动参数"
        nameFilters: ["mksync 启动参数 (*.toml)", "所有文件 (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: root.runtime.importStartupConfig(selectedFile)
    }
    FileDialog {
        id: startupExportDialog
        title: "导出命令行启动参数"
        nameFilters: ["mksync 启动参数 (*.toml)", "所有文件 (*)"]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "toml"
        onAccepted: root.runtime.saveStartupConfigAs(selectedFile)
    }
    FileDialog {
        id: screenOpenDialog
        title: "打开屏幕配置"
        nameFilters: ["mksync 屏幕配置 (*.json)", "所有文件 (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: root.presenter.importConfig(selectedFile)
    }
    FileDialog {
        id: screenSaveDialog
        title: "保存屏幕配置"
        nameFilters: ["mksync 屏幕配置 (*.json)", "所有文件 (*)"]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        onAccepted: root.presenter.saveAs(selectedFile)
    }

    ScrollView {
        anchors.fill: parent
        clip: true

        ColumnLayout {
            width: root.width
            spacing: 10

            ColumnLayout {
                spacing: 3
                Text {
                    text: "配置"
                    color: "#20242c"
                    font.pixelSize: root.narrowLayout ? 20 : 23
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "启动参数使用 CLI 的 TOML 格式；屏幕布局继续使用独立的 JSON 文件。"
                    color: "#727984"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }

            Components.SectionCard {
                Layout.fillWidth: true

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "命令行启动参数"
                            color: "#303640"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Item { Layout.fillWidth: true }
                        Text {
                            text: root.runtime.startupConfigPath.length === 0
                                ? "尚未导出"
                                : root.runtime.startupConfigPath
                            color: "#7b828c"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.maximumWidth: root.narrowLayout ? 190 : 360
                        }
                    }

                    GridLayout {
                        Layout.fillWidth: true
                        columns: root.narrowLayout ? 2 : 3
                        columnSpacing: 12
                        rowSpacing: 9

                        Text {
                            text: "运行模式"
                            color: "#626a75"
                            font.pixelSize: 12
                            Layout.preferredWidth: 92
                        }
                        RowLayout {
                            Layout.columnSpan: root.narrowLayout ? 1 : 2
                            Layout.fillWidth: true
                            RadioButton {
                                text: "server"
                                checked: root.runtime.selectedMode === 0
                                enabled: !root.runtime.active
                                onClicked: root.runtime.selectedMode = 0
                                font.pixelSize: 12
                            }
                            RadioButton {
                                text: "client"
                                checked: root.runtime.selectedMode === 1
                                enabled: !root.runtime.active
                                onClicked: root.runtime.selectedMode = 1
                                font.pixelSize: 12
                            }
                            Item { Layout.fillWidth: true }
                        }

                        Text {
                            text: "输入后端"
                            color: "#626a75"
                            font.pixelSize: 12
                        }
                        ComboBox {
                            model: root.runtime.backendOptions
                            currentIndex: Math.max(
                                0, root.runtime.backendOptions.indexOf(
                                    root.runtime.selectedBackend))
                            enabled: !root.runtime.active
                            implicitHeight: 34
                            Layout.preferredWidth: 220
                            onActivated: root.runtime.selectedBackend = currentText
                        }
                        Text {
                            visible: !root.narrowLayout
                            text: "auto 按注册顺序选择"
                            color: "#8a919b"
                            font.pixelSize: 11
                        }

                        Text {
                            text: "网络端点"
                            color: "#626a75"
                            font.pixelSize: 12
                        }
                        Components.MksTextField {
                            id: endpointField
                            enabled: !root.runtime.active
                            placeholderText: "0.0.0.0:1234"
                            Layout.fillWidth: true
                            Layout.columnSpan: root.narrowLayout ? 1 : 2
                            onEditingFinished: {
                                root.runtime.endpoint = text
                                root.refreshEndpointField()
                            }
                        }

                        Text {
                            text: "日志等级"
                            color: "#626a75"
                            font.pixelSize: 12
                        }
                        ComboBox {
                            model: root.logLevels
                            currentIndex: Math.max(0, root.logLevels.indexOf(root.runtime.logLevel))
                            enabled: !root.runtime.active
                            implicitHeight: 34
                            Layout.preferredWidth: 160
                            onActivated: root.runtime.logLevel = currentText
                        }
                        Text {
                            visible: !root.narrowLayout
                            text: "对应 --log-level"
                            color: "#8a919b"
                            font.pixelSize: 11
                        }

                        Text {
                            text: "屏幕配置路径"
                            color: "#626a75"
                            font.pixelSize: 12
                        }
                        Text {
                            text: root.runtime.appConfigPath.length === 0
                                ? "尚未指定"
                                : root.runtime.appConfigPath
                            color: root.runtime.appConfigPath.length === 0 ? "#a06f13" : "#4f5661"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                            Layout.columnSpan: root.narrowLayout ? 1 : 2
                        }
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e5e7ea" }

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "后端能力检查"
                            color: "#4f5661"
                            font.pixelSize: 12
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Components.MksButton {
                            text: root.runtime.backendChecking ? "检查中…" : "重新检查"
                            secondary: true
                            enabled: !root.runtime.backendChecking && !root.runtime.active
                            implicitWidth: 86
                            implicitHeight: 28
                            onClicked: root.runtime.refreshBackendChecks()
                        }
                    }
                    Text {
                        text: root.runtime.backendCheckText
                        color: "#626a75"
                        font.family: "monospace"
                        font.pixelSize: 10
                        wrapMode: Text.WrapAnywhere
                        Layout.fillWidth: true
                    }

                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e5e7ea" }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Components.MksButton {
                            text: "新建"
                            secondary: true
                            enabled: !root.runtime.active
                            onClicked: root.runtime.createStartupConfig()
                        }
                        Components.MksButton {
                            text: "导入…"
                            secondary: true
                            enabled: !root.runtime.active
                            onClicked: startupImportDialog.open()
                        }
                        Components.MksButton {
                            text: "导出…"
                            secondary: true
                            onClicked: startupExportDialog.open()
                        }
                        Item { Layout.fillWidth: true }
                        Components.MksButton {
                            text: "重新导出"
                            enabled: root.runtime.startupConfigPath.length > 0
                            onClicked: root.runtime.saveStartupConfig()
                        }
                    }

                    Text {
                        text: root.runtime.startupStatusMessage
                        color: root.runtime.hasStartupError ? "#a92e2e" : "#59616d"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Components.SectionCard {
                Layout.fillWidth: true

                ColumnLayout {
                    width: parent.width
                    spacing: 10

                    Text {
                        text: "屏幕与可信设备配置"
                        color: "#303640"
                        font.pixelSize: 14
                        font.weight: Font.DemiBold
                    }
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 2
                        columnSpacing: 12
                        rowSpacing: 8
                        Text { text: "文件位置"; color: "#6f7681"; font.pixelSize: 12 }
                        Text {
                            text: root.presenter.configPath.length === 0
                                ? "尚未保存到文件"
                                : root.presenter.configPath
                            color: root.presenter.configPath.length === 0 ? "#9a721e" : "#303640"
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                        Text { text: "本机设备 ID"; color: "#6f7681"; font.pixelSize: 12 }
                        Text {
                            text: root.presenter.machineId
                            color: "#303640"
                            font.pixelSize: 11
                            font.family: "monospace"
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 8
                        Components.MksButton {
                            text: "新建"
                            secondary: true
                            onClicked: root.presenter.createNew()
                        }
                        Components.MksButton {
                            text: "打开…"
                            secondary: true
                            onClicked: screenOpenDialog.open()
                        }
                        Components.MksButton {
                            text: "另存为…"
                            secondary: true
                            onClicked: screenSaveDialog.open()
                        }
                        Item { Layout.fillWidth: true }
                        Components.MksButton {
                            text: "保存 JSON"
                            onClicked: root.presenter.save()
                        }
                    }
                    Text {
                        text: "TOML 的 params.common.configPath 指向此 JSON 文件；两者用途不同。"
                        color: "#727984"
                        font.pixelSize: 11
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }

            Item { Layout.preferredHeight: 2 }
        }
    }
}
