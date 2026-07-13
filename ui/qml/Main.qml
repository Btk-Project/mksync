import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "components" as Components
import "pages" as Pages

ApplicationWindow {
    id: window

    width: 1040
    height: 680
    minimumWidth: 560
    minimumHeight: 560
    visible: true
    title: "mksync"
    color: "#f5f6f8"

    property int currentPage: 0
    readonly property bool narrowLayout: width < 820
    readonly property var navigation: [
        { "title": "运行", "shortcut": "1", "mark": "●" },
        { "title": "屏幕布局", "shortcut": "2", "mark": "▣" },
        { "title": "可信设备", "shortcut": "3", "mark": "✓" },
        { "title": "配置", "shortcut": "4", "mark": "≡" }
    ]

    onClosing: function(close) {
        if (runtimePresenter.active) {
            close.accepted = false
            runtimePresenter.quitApplication()
        }
    }

    Shortcut {
        sequence: "Ctrl+S"
        enabled: window.currentPage === 1 || window.currentPage === 2
        onActivated: settingsPresenter.save()
    }
    Shortcut { sequence: "Ctrl+1"; onActivated: window.currentPage = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: window.currentPage = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: window.currentPage = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: window.currentPage = 3 }

    header: ToolBar {
        implicitHeight: 46
        background: Rectangle {
            color: "#ffffff"
            border.color: "#dfe2e7"
            border.width: 1
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: window.narrowLayout ? 12 : 16
            anchors.rightMargin: 12
            spacing: 10

            Rectangle {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                radius: 4
                color: "#356ae6"

                Text {
                    anchors.centerIn: parent
                    text: "m"
                    color: "white"
                    font.pixelSize: 16
                    font.weight: Font.Bold
                }
            }
            Text {
                text: "mksync"
                color: "#20242c"
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 20
                color: "#dfe2e7"
                Layout.leftMargin: 4
                Layout.rightMargin: 4
            }
            Text {
                text: window.navigation[window.currentPage].title
                color: "#606773"
                font.pixelSize: 13
                Layout.fillWidth: true
            }
            Components.MksButton {
                text: window.narrowLayout ? "保存" : "保存配置"
                visible: window.currentPage === 1 || window.currentPage === 2
                implicitWidth: window.narrowLayout ? 68 : 88
                implicitHeight: 30
                onClicked: settingsPresenter.save()
            }
        }
    }

    footer: Rectangle {
        implicitHeight: 30
        color: "#ffffff"
        border.color: "#dfe2e7"
        border.width: 1

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 14
            anchors.rightMargin: 14
            spacing: 9

            Rectangle {
                Layout.preferredWidth: 7
                Layout.preferredHeight: 7
                radius: 4
                color: window.currentPage === 0
                    ? (runtimePresenter.running ? "#2f9e62" : "#a1a7b0")
                    : (settingsPresenter.hasError ? "#d14343" : "#2f9e62")
            }
            Text {
                text: window.currentPage === 0
                    ? runtimePresenter.statusTitle + " · " + runtimePresenter.statusDetail
                    : settingsPresenter.statusMessage
                color: window.currentPage !== 0 && settingsPresenter.hasError ? "#a92e2e" : "#59616d"
                font.pixelSize: 12
                elide: Text.ElideRight
                Layout.fillWidth: true
            }
            Text {
                visible: !window.narrowLayout && window.currentPage !== 0
                text: settingsPresenter.configPath.length === 0
                    ? "未保存"
                    : settingsPresenter.configPath
                color: "#858c96"
                font.pixelSize: 11
                elide: Text.ElideMiddle
                Layout.maximumWidth: 360
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            visible: !window.narrowLayout
            Layout.fillHeight: true
            Layout.preferredWidth: 174
            color: "#eceef1"
            border.color: "#d9dce2"
            border.width: 1

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 11
                anchors.bottomMargin: 10
                spacing: 2

                Text {
                    text: "配置"
                    color: "#858c96"
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    Layout.leftMargin: 16
                    Layout.bottomMargin: 5
                }

                Repeater {
                    model: window.navigation

                    delegate: ItemDelegate {
                        id: navigationDelegate
                        required property var modelData
                        required property int index

                        Layout.fillWidth: true
                        Layout.leftMargin: 6
                        Layout.rightMargin: 6
                        implicitHeight: 35
                        hoverEnabled: true
                        onClicked: window.currentPage = index

                        contentItem: RowLayout {
                            spacing: 10
                            Text {
                                text: modelData.mark
                                color: window.currentPage === index ? "#285bc5" : "#6f7681"
                                font.pixelSize: 15
                                Layout.preferredWidth: 18
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Text {
                                text: modelData.title
                                color: window.currentPage === index ? "#1f2937" : "#4f5661"
                                font.pixelSize: 13
                                font.weight: window.currentPage === index ? Font.DemiBold : Font.Normal
                                Layout.fillWidth: true
                            }
                            Text {
                                text: modelData.shortcut
                                color: "#9aa0a9"
                                font.pixelSize: 11
                            }
                        }
                        background: Rectangle {
                            radius: 4
                            color: window.currentPage === index
                                ? "#ffffff"
                                : (navigationDelegate.hovered ? "#e3e6ea" : "transparent")
                            border.color: window.currentPage === index ? "#d7dae0" : "transparent"
                        }
                    }
                }

                Item { Layout.fillHeight: true }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.leftMargin: 12
                    Layout.rightMargin: 12
                    implicitHeight: 1
                    color: "#d5d8de"
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.leftMargin: 16
                    Layout.rightMargin: 16
                    Layout.topMargin: 8
                    spacing: 3

                    Text {
                        text: "本机设备 ID"
                        color: "#858c96"
                        font.pixelSize: 11
                    }
                    Text {
                        text: settingsPresenter.machineId
                        color: "#4d5560"
                        font.pixelSize: 11
                        font.family: "monospace"
                        elide: Text.ElideMiddle
                        Layout.fillWidth: true
                    }
                }
            }
        }

        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#f5f6f8"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                Rectangle {
                    visible: window.narrowLayout
                    Layout.fillWidth: true
                    Layout.preferredHeight: visible ? 42 : 0
                    color: "#eceef1"
                    border.color: "#d9dce2"

                    RowLayout {
                        anchors.fill: parent
                        anchors.margins: 4
                        spacing: 3

                        Repeater {
                            model: window.navigation

                            delegate: ItemDelegate {
                                id: compactNavigationDelegate
                                required property var modelData
                                required property int index

                                Layout.fillWidth: true
                                Layout.fillHeight: true
                                hoverEnabled: true
                                onClicked: window.currentPage = index

                                contentItem: RowLayout {
                                    spacing: 5
                                    Text {
                                        text: modelData.mark
                                        color: window.currentPage === index ? "#285bc5" : "#737a85"
                                        font.pixelSize: 12
                                    }
                                    Text {
                                        text: modelData.title
                                        color: window.currentPage === index ? "#20242c" : "#5d6570"
                                        font.pixelSize: 11
                                        font.weight: window.currentPage === index
                                            ? Font.DemiBold : Font.Normal
                                        elide: Text.ElideRight
                                        Layout.fillWidth: true
                                    }
                                }
                                background: Rectangle {
                                    radius: 3
                                    color: window.currentPage === index
                                        ? "#ffffff"
                                        : (compactNavigationDelegate.hovered ? "#e2e5e9" : "transparent")
                                    border.color: window.currentPage === index ? "#d4d8de" : "transparent"
                                }
                            }
                        }
                    }
                }

                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true

                    StackLayout {
                        anchors.fill: parent
                        anchors.leftMargin: window.narrowLayout ? 12 : 22
                        anchors.rightMargin: window.narrowLayout ? 12 : 22
                        anchors.topMargin: window.narrowLayout ? 10 : 16
                        anchors.bottomMargin: window.narrowLayout ? 10 : 16
                        currentIndex: window.currentPage

                        Pages.HomePage {
                            runtime: runtimePresenter
                            onConfigureRequested: window.currentPage = 3
                        }
                        Pages.WorkspacePage {
                            presenter: settingsPresenter
                            runtime: runtimePresenter
                        }
                        Pages.TrustedClientsPage { presenter: settingsPresenter }
                        Pages.ConfigurationPage {
                            presenter: settingsPresenter
                            runtime: runtimePresenter
                        }
                    }
                }
            }
        }
    }
}
