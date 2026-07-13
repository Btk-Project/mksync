import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var presenter
    readonly property bool narrowLayout: width < 680

    FileDialog {
        id: importDialog
        title: "导入 mksync 配置"
        nameFilters: ["mksync 配置 (*.json)", "所有文件 (*)"]
        fileMode: FileDialog.OpenFile
        onAccepted: root.presenter.importConfig(selectedFile)
    }
    FileDialog {
        id: exportDialog
        title: "保存 mksync 配置"
        nameFilters: ["mksync 配置 (*.json)", "所有文件 (*)"]
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        onAccepted: root.presenter.saveAs(selectedFile)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 10

        ColumnLayout {
            spacing: 3
            Text {
                text: "配置文件"
                color: "#20242c"
                font.pixelSize: root.narrowLayout ? 20 : 23
                font.weight: Font.DemiBold
            }
            Text {
                text: "GUI 与命令行程序使用同一份配置，无需导出或转换。"
                color: "#727984"
                font.pixelSize: 13
            }
        }

        Components.SectionCard {
            Layout.fillWidth: true

            ColumnLayout {
                width: parent.width
                spacing: 0

                Text {
                    text: "当前配置"
                    color: "#303640"
                    font.pixelSize: 14
                    font.weight: Font.DemiBold
                    Layout.bottomMargin: 12
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 52
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent
                        spacing: 12
                        Text {
                            text: "文件位置"
                            color: "#6f7681"
                            font.pixelSize: 12
                            Layout.preferredWidth: 92
                        }
                        Text {
                            text: root.presenter.configPath.length === 0
                                ? "尚未保存到文件"
                                : root.presenter.configPath
                            color: root.presenter.configPath.length === 0 ? "#9a721e" : "#303640"
                            font.pixelSize: 12
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e6e8eb" }
                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 52
                    color: "transparent"
                    RowLayout {
                        anchors.fill: parent
                        spacing: 12
                        Text {
                            text: "本机设备 ID"
                            color: "#6f7681"
                            font.pixelSize: 12
                            Layout.preferredWidth: 92
                        }
                        Text {
                            text: root.presenter.machineId
                            color: "#303640"
                            font.pixelSize: 12
                            font.family: "monospace"
                            elide: Text.ElideMiddle
                            Layout.fillWidth: true
                        }
                    }
                }
                Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e6e8eb" }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.topMargin: 16
                    spacing: 8
                    Components.MksButton {
                        text: "新建"
                        secondary: true
                        onClicked: root.presenter.createNew()
                    }
                    Components.MksButton {
                        text: "打开…"
                        secondary: true
                        onClicked: importDialog.open()
                    }
                    Components.MksButton {
                        text: "另存为…"
                        secondary: true
                        onClicked: exportDialog.open()
                    }
                    Item { Layout.fillWidth: true }
                    Components.MksButton {
                        text: "保存"
                        onClicked: root.presenter.save()
                    }
                }
            }
        }

        Components.SectionCard {
            Layout.fillWidth: true

            RowLayout {
                width: parent.width
                spacing: 14
                Rectangle {
                    Layout.preferredWidth: 32
                    Layout.preferredHeight: 32
                    radius: 16
                    color: "#e8efff"
                    Text {
                        anchors.centerIn: parent
                        text: "i"
                        color: "#356ae6"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 3
                    Text {
                        text: "命令行兼容"
                        color: "#303640"
                        font.pixelSize: 13
                        font.weight: Font.DemiBold
                    }
                    Text {
                        text: "此文件可直接通过 mksync server 或 mksync client 的 --config 参数使用。"
                        color: "#727984"
                        font.pixelSize: 12
                        wrapMode: Text.WordWrap
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
