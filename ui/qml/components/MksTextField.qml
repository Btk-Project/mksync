import QtQuick
import QtQuick.Controls

TextField {
    id: control

    implicitHeight: 32
    color: "#252a31"
    placeholderTextColor: "#949aa3"
    selectedTextColor: "#ffffff"
    selectionColor: "#356ae6"
    font.pixelSize: 13
    selectByMouse: true
    leftPadding: 10
    rightPadding: 10

    background: Rectangle {
        radius: 3
        color: control.enabled ? "#ffffff" : "#f1f2f4"
        border.width: control.activeFocus ? 2 : 1
        border.color: control.activeFocus ? "#356ae6" : "#cbd0d7"
    }
}
