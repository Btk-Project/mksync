import QtQuick
import QtQuick.Controls

Button {
    id: control

    property bool secondary: false
    property bool destructive: false

    implicitWidth: Math.max(82, contentItem.implicitWidth + 22)
    implicitHeight: 32
    hoverEnabled: true
    font.pixelSize: 13
    font.weight: Font.Medium

    contentItem: Text {
        text: control.text
        color: {
            if (!control.enabled)
                return "#a5abb4"
            if (control.destructive)
                return "#b33a3a"
            return control.secondary ? "#3f4650" : "#ffffff"
        }
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        font: control.font
    }

    background: Rectangle {
        radius: 4
        color: {
            if (!control.enabled)
                return control.secondary ? "#f2f3f5" : "#9eb5e8"
            if (control.destructive)
                return control.down ? "#f6dada" : (control.hovered ? "#fbe8e8" : "#ffffff")
            if (control.secondary)
                return control.down ? "#e4e7eb" : (control.hovered ? "#f0f1f3" : "#ffffff")
            return control.down ? "#244fae" : (control.hovered ? "#2e60ce" : "#356ae6")
        }
        border.width: control.secondary || control.destructive ? 1 : 0
        border.color: control.destructive ? "#e4bcbc" : "#cfd3d9"
    }
}
