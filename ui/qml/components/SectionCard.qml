import QtQuick

Rectangle {
    id: control

    default property alias content: contentHost.data
    property int contentMargins: 14

    radius: 5
    color: "#ffffff"
    border.width: 1
    border.color: "#dadde2"
    implicitHeight: contentHost.implicitHeight + contentMargins * 2

    Item {
        id: contentHost
        anchors.fill: parent
        anchors.margins: control.contentMargins
        implicitWidth: childrenRect.width
        implicitHeight: childrenRect.height
    }
}
