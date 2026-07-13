import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var presenter
    readonly property bool narrowLayout: width < 680

    Dialog {
        id: addClientDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(400, root.width - 24)
        title: "添加可信设备"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            clientIdField.clear()
            nameField.clear()
            clientIdField.forceActiveFocus()
        }
        onAccepted: root.presenter.addTrustedClient(clientIdField.text, nameField.text)

        background: Rectangle {
            radius: 6
            color: "#ffffff"
            border.color: "#cfd3d9"
        }
        contentItem: ColumnLayout {
            spacing: 7
            Text {
                text: "加入列表后，服务端只接受匹配设备 ID 或名称的客户端。"
                color: "#676e78"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.bottomMargin: 5
                font.pixelSize: 12
            }
            Text { text: "设备 ID"; color: "#3f4650"; font.pixelSize: 12 }
            Components.MksTextField {
                id: clientIdField
                placeholderText: "远端设备的 machine ID"
                Layout.fillWidth: true
            }
            Text {
                text: "显示名称"
                color: "#3f4650"
                font.pixelSize: 12
                Layout.topMargin: 3
            }
            Components.MksTextField {
                id: nameField
                placeholderText: "例如：书房电脑"
                Layout.fillWidth: true
            }
        }
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
                    text: "可信设备"
                    color: "#20242c"
                    font.pixelSize: root.narrowLayout ? 20 : 23
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "限制可以连接并接收键鼠输入的设备。"
                    color: "#727984"
                    font.pixelSize: 13
                }
            }
            Components.MksButton {
                text: "添加设备"
                onClicked: addClientDialog.open()
            }
        }

        Rectangle {
            Layout.fillWidth: true
            implicitHeight: 50
            radius: 4
            color: root.presenter.trustedClients.length === 0 ? "#fff8e7" : "#eef5ff"
            border.color: root.presenter.trustedClients.length === 0 ? "#ead9aa" : "#cbdcf7"

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                spacing: 11
                Rectangle {
                    Layout.preferredWidth: 8
                    Layout.preferredHeight: 8
                    radius: 4
                    color: root.presenter.trustedClients.length === 0 ? "#d29a28" : "#356ae6"
                }
                Text {
                    text: root.presenter.trustedClients.length === 0 ? "当前允许所有客户端" : "白名单已启用"
                    color: "#343a43"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                }
                Text {
                    text: root.presenter.trustedClients.length === 0
                        ? "添加至少一台设备即可启用连接限制。"
                        : "仅列表中的设备可以通过服务端检查。"
                    color: "#6f7680"
                    font.pixelSize: 12
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

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: 38
                    color: "#f3f4f6"
                    radius: 5

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 16
                        anchors.rightMargin: 12
                        spacing: 12
                        Text {
                            text: "设备名称"
                            color: "#626a75"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            Layout.preferredWidth: root.narrowLayout ? 120 : 210
                        }
                        Text {
                            text: "设备 ID"
                            color: "#626a75"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Item { Layout.preferredWidth: 70 }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    Column {
                        anchors.centerIn: parent
                        spacing: 6
                        visible: root.presenter.trustedClients.length === 0
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "没有可信设备"
                            color: "#545b65"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                        }
                        Text {
                            anchors.horizontalCenter: parent.horizontalCenter
                            text: "当前处于开放连接模式"
                            color: "#9298a1"
                            font.pixelSize: 12
                        }
                    }

                    ListView {
                        anchors.fill: parent
                        clip: true
                        model: root.presenter.trustedClients

                        delegate: Rectangle {
                            required property var modelData
                            required property int index

                            width: ListView.view.width
                            height: 50
                            color: index % 2 === 0 ? "#ffffff" : "#fafbfc"

                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 1
                                color: "#eceef1"
                            }
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 16
                                anchors.rightMargin: 12
                                spacing: 12
                                Text {
                                    text: modelData.name.length === 0 ? "未命名设备" : modelData.name
                                    color: "#303640"
                                    font.pixelSize: 12
                                    font.weight: Font.Medium
                                    elide: Text.ElideRight
                                    Layout.preferredWidth: root.narrowLayout ? 120 : 210
                                }
                                Text {
                                    text: modelData.machineId.length === 0 ? "未指定" : modelData.machineId
                                    color: "#727984"
                                    font.pixelSize: 11
                                    font.family: "monospace"
                                    elide: Text.ElideMiddle
                                    Layout.fillWidth: true
                                }
                                Components.MksButton {
                                    text: "移除"
                                    destructive: true
                                    implicitWidth: 62
                                    implicitHeight: 28
                                    onClicked: root.presenter.removeTrustedClient(index)
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
