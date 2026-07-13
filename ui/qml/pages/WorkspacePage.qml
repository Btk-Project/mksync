import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../components" as Components

Item {
    id: root

    required property var presenter
    required property var runtime
    readonly property bool narrowLayout: width < 720

    Shortcut {
        sequences: ["Ctrl++", "Ctrl+="]
        enabled: root.visible
        onActivated: layoutArea.zoomBy(1.18)
    }
    Shortcut {
        sequence: "Ctrl+-"
        enabled: root.visible
        onActivated: layoutArea.zoomBy(1 / 1.18)
    }
    Shortcut {
        sequence: "Ctrl+0"
        enabled: root.visible && layoutArea.screenEntries.length > 0
        onActivated: layoutArea.fitAll()
    }

    Dialog {
        id: addScreenDialog
        modal: true
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: Math.min(410, root.width - 24)
        title: "添加屏幕"
        standardButtons: Dialog.Ok | Dialog.Cancel

        onOpened: {
            ownerIdField.text = root.presenter.machineId
            screenIndexField.text = "0"
            xField.text = "0"
            yField.text = "0"
            ownerIdField.forceActiveFocus()
        }
        onAccepted: root.presenter.addScreen(
            ownerIdField.text,
            Number(screenIndexField.text),
            Number(xField.text),
            Number(yField.text)
        )

        background: Rectangle {
            radius: 6
            color: "#ffffff"
            border.color: "#cfd3d9"
        }

        contentItem: ColumnLayout {
            spacing: 9

            Text {
                text: "屏幕由设备 ID 和屏幕编号唯一标识。坐标决定鼠标跨屏方向。"
                color: "#676e78"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                Layout.bottomMargin: 5
                font.pixelSize: 12
            }
            Text { text: "设备 ID"; color: "#3f4650"; font.pixelSize: 12 }
            Components.MksTextField { id: ownerIdField; Layout.fillWidth: true }

            RowLayout {
                Layout.fillWidth: true
                spacing: 10

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Text { text: "屏幕编号"; color: "#3f4650"; font.pixelSize: 12 }
                    Components.MksTextField {
                        id: screenIndexField
                        inputMethodHints: Qt.ImhDigitsOnly
                        Layout.fillWidth: true
                    }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Text { text: "横坐标"; color: "#3f4650"; font.pixelSize: 12 }
                    Components.MksTextField { id: xField; Layout.fillWidth: true }
                }
                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 5
                    Text { text: "纵坐标"; color: "#3f4650"; font.pixelSize: 12 }
                    Components.MksTextField { id: yField; Layout.fillWidth: true }
                }
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
                    text: "屏幕布局"
                    color: "#20242c"
                    font.pixelSize: root.narrowLayout ? 20 : 23
                    font.weight: Font.DemiBold
                }
                Text {
                    text: "拖动屏幕设置相对位置；鼠标越过边缘时会进入相邻设备。"
                    color: "#727984"
                    font.pixelSize: 13
                    wrapMode: Text.WordWrap
                    Layout.fillWidth: true
                }
            }
            Components.MksButton {
                text: "添加屏幕"
                onClicked: addScreenDialog.open()
            }
        }

        GridLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            columns: root.narrowLayout ? 1 : 2
            rowSpacing: 10
            columnSpacing: 10

            Components.SectionCard {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumHeight: root.narrowLayout ? 260 : 0
                contentMargins: 0

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 42
                        color: "#ffffff"
                        radius: 5

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 14
                            anchors.rightMargin: 14
                            spacing: 8
                            Text {
                                text: "拓扑预览"
                                color: "#303640"
                                font.pixelSize: 13
                                font.weight: Font.DemiBold
                            }
                            Item { Layout.fillWidth: true }
                            Components.MksButton {
                                text: "−"
                                secondary: true
                                implicitWidth: 30
                                implicitHeight: 28
                                font.pixelSize: 16
                                enabled: layoutArea.zoom > layoutArea.minimumZoom + 0.01
                                onClicked: layoutArea.zoomBy(1 / 1.18)
                                ToolTip.visible: hovered
                                ToolTip.text: "缩小"
                            }
                            Text {
                                Layout.preferredWidth: 40
                                text: Math.round(layoutArea.zoom * 100) + "%"
                                color: "#626a75"
                                font.pixelSize: 11
                                horizontalAlignment: Text.AlignHCenter
                            }
                            Components.MksButton {
                                text: "+"
                                secondary: true
                                implicitWidth: 30
                                implicitHeight: 28
                                font.pixelSize: 15
                                enabled: layoutArea.zoom < layoutArea.maximumZoom - 0.01
                                onClicked: layoutArea.zoomBy(1.18)
                                ToolTip.visible: hovered
                                ToolTip.text: "放大"
                            }
                            Components.MksButton {
                                text: "适应"
                                secondary: true
                                implicitWidth: 54
                                implicitHeight: 28
                                enabled: layoutArea.screenEntries.length > 0
                                onClicked: layoutArea.fitAll()
                                ToolTip.visible: hovered
                                ToolTip.text: "缩放并显示全部屏幕"
                            }
                            Components.MksButton {
                                text: root.narrowLayout ? "定位" : "定位当前"
                                secondary: true
                                implicitWidth: root.narrowLayout ? 54 : 72
                                implicitHeight: 28
                                enabled: layoutArea.screenEntries.length > 0
                                onClicked: layoutArea.locateActive()
                                ToolTip.visible: hovered
                                ToolTip.text: root.runtime.hasActiveScreen
                                    ? "定位当前正在接收键鼠输入的屏幕"
                                    : "服务未运行时定位选中或本机屏幕"
                            }
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e1e4e8" }

                    Rectangle {
                        id: layoutArea
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        color: "#f8f9fa"
                        clip: true

                        property var screenEntries: root.presenter.screens
                        property real cellWidth: 132
                        property real cellHeight: 102
                        property real zoom: 1
                        readonly property real minimumZoom: 0.3
                        readonly property real maximumZoom: 2.2
                        property real panX: 0
                        property real panY: 0
                        property int selectedIndex: -1
                        property bool hasFittedScreens: false

                        function boundedZoom(value) {
                            return Math.max(minimumZoom, Math.min(maximumZoom, value))
                        }

                        function setZoomAt(value, anchorX, anchorY) {
                            var nextZoom = boundedZoom(value)
                            if (Math.abs(nextZoom - zoom) < 0.001)
                                return

                            var worldX = (anchorX - width / 2 - panX) / zoom
                            var worldY = (anchorY - height / 2 - panY) / zoom
                            panX = anchorX - width / 2 - worldX * nextZoom
                            panY = anchorY - height / 2 - worldY * nextZoom
                            zoom = nextZoom
                        }

                        function zoomBy(factor) {
                            setZoomAt(zoom * factor, width / 2, height / 2)
                        }

                        function fitAll() {
                            if (screenEntries.length === 0 || width <= 0 || height <= 0)
                                return

                            var minimumX = screenEntries[0].x
                            var maximumX = minimumX
                            var minimumY = screenEntries[0].y
                            var maximumY = minimumY
                            for (var i = 1; i < screenEntries.length; ++i) {
                                minimumX = Math.min(minimumX, screenEntries[i].x)
                                maximumX = Math.max(maximumX, screenEntries[i].x)
                                minimumY = Math.min(minimumY, screenEntries[i].y)
                                maximumY = Math.max(maximumY, screenEntries[i].y)
                            }

                            var contentWidth = (maximumX - minimumX) * cellWidth + 116
                            var contentHeight = (maximumY - minimumY) * cellHeight + 78
                            var availableWidth = Math.max(1, width - 48)
                            var availableHeight = Math.max(1, height - 48)
                            zoom = boundedZoom(Math.min(1.35, availableWidth / contentWidth,
                                                        availableHeight / contentHeight))
                            panX = -(minimumX + maximumX) * cellWidth * zoom / 2
                            panY = -(minimumY + maximumY) * cellHeight * zoom / 2
                            hasFittedScreens = true
                        }

                        function focusIndex(index) {
                            if (index < 0 || index >= screenEntries.length)
                                return
                            selectedIndex = index
                            if (zoom < 0.75)
                                zoom = 0.9
                            panX = -screenEntries[index].x * cellWidth * zoom
                            panY = -screenEntries[index].y * cellHeight * zoom
                        }

                        function locateActive() {
                            var targetIndex = -1
                            if (root.runtime.hasActiveScreen) {
                                for (var i = 0; i < screenEntries.length; ++i) {
                                    if (screenEntries[i].ownerId === root.runtime.activeScreenOwnerId
                                            && screenEntries[i].screenIndex
                                                === root.runtime.activeScreenIndex) {
                                        targetIndex = i
                                        break
                                    }
                                }
                            }
                            if (targetIndex < 0 && selectedIndex >= 0
                                    && selectedIndex < screenEntries.length)
                                targetIndex = selectedIndex
                            if (targetIndex < 0) {
                                for (var j = 0; j < screenEntries.length; ++j) {
                                    if (screenEntries[j].isLocal) {
                                        targetIndex = j
                                        break
                                    }
                                }
                            }
                            if (targetIndex < 0 && screenEntries.length > 0)
                                targetIndex = 0
                            focusIndex(targetIndex)
                        }

                        function isActiveScreen(screen) {
                            return root.runtime.hasActiveScreen
                                && screen.ownerId === root.runtime.activeScreenOwnerId
                                && screen.screenIndex === root.runtime.activeScreenIndex
                        }

                        function screenItemX(screen) {
                            return width / 2 + panX + screen.x * cellWidth * zoom - 58
                        }

                        function screenItemY(screen) {
                            return height / 2 + panY + screen.y * cellHeight * zoom - 39
                        }

                        function applyWheel(wheel, sourceItem) {
                            var point = sourceItem.mapToItem(layoutArea, wheel.x, wheel.y)
                            setZoomAt(zoom * (wheel.angleDelta.y > 0 ? 1.12 : 1 / 1.12),
                                      point.x, point.y)
                            wheel.accepted = true
                        }

                        Component.onCompleted: Qt.callLater(fitAll)

                        Connections {
                            target: root.presenter
                            function onScreensChanged() {
                                if (layoutArea.screenEntries.length === 0) {
                                    layoutArea.selectedIndex = -1
                                    layoutArea.hasFittedScreens = false
                                } else if (!layoutArea.hasFittedScreens) {
                                    Qt.callLater(layoutArea.fitAll)
                                } else if (layoutArea.selectedIndex
                                           >= layoutArea.screenEntries.length) {
                                    layoutArea.selectedIndex = -1
                                }
                            }
                        }

                        Repeater {
                            model: Math.ceil(layoutArea.width
                                             / (layoutArea.cellWidth * layoutArea.zoom)) + 3
                            delegate: Rectangle {
                                required property int index
                                readonly property real spacing: layoutArea.cellWidth * layoutArea.zoom
                                x: ((layoutArea.width / 2 + layoutArea.panX) % spacing + spacing)
                                   % spacing + (index - 1) * spacing
                                y: 0
                                width: 1
                                height: layoutArea.height
                                color: "#e9ebee"
                            }
                        }
                        Repeater {
                            model: Math.ceil(layoutArea.height
                                             / (layoutArea.cellHeight * layoutArea.zoom)) + 3
                            delegate: Rectangle {
                                required property int index
                                readonly property real spacing: layoutArea.cellHeight
                                                                * layoutArea.zoom
                                x: 0
                                y: ((layoutArea.height / 2 + layoutArea.panY) % spacing + spacing)
                                   % spacing + (index - 1) * spacing
                                width: layoutArea.width
                                height: 1
                                color: "#e9ebee"
                            }
                        }
                        Rectangle {
                            x: layoutArea.width / 2 + layoutArea.panX
                            y: 0
                            width: 1
                            height: layoutArea.height
                            color: "#cfd4da"
                        }
                        Rectangle {
                            x: 0
                            y: layoutArea.height / 2 + layoutArea.panY
                            width: layoutArea.width
                            height: 1
                            color: "#cfd4da"
                        }

                        MouseArea {
                            id: canvasPanArea
                            anchors.fill: parent
                            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
                            cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                            property real previousX: 0
                            property real previousY: 0
                            property bool moved: false

                            onPressed: function(mouse) {
                                previousX = mouse.x
                                previousY = mouse.y
                                moved = false
                            }
                            onPositionChanged: function(mouse) {
                                if (!pressed)
                                    return
                                var deltaX = mouse.x - previousX
                                var deltaY = mouse.y - previousY
                                layoutArea.panX += deltaX
                                layoutArea.panY += deltaY
                                moved = moved || Math.abs(deltaX) > 0.5 || Math.abs(deltaY) > 0.5
                                previousX = mouse.x
                                previousY = mouse.y
                            }
                            onClicked: {
                                if (!moved)
                                    layoutArea.selectedIndex = -1
                            }
                            onWheel: function(wheel) {
                                layoutArea.applyWheel(wheel, canvasPanArea)
                            }
                        }

                        Column {
                            anchors.centerIn: parent
                            spacing: 7
                            visible: layoutArea.screenEntries.length === 0

                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "▱"
                                color: "#a0a7b1"
                                font.pixelSize: 36
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "尚未配置屏幕"
                                color: "#4f5661"
                                font.pixelSize: 14
                                font.weight: Font.DemiBold
                            }
                            Text {
                                anchors.horizontalCenter: parent.horizontalCenter
                                text: "添加本机屏幕，然后将远端屏幕放到它的四周"
                                color: "#8a919b"
                                font.pixelSize: 12
                            }
                        }

                        Repeater {
                            model: layoutArea.screenEntries

                            delegate: Item {
                                id: screenTile
                                required property var modelData
                                required property int index

                                property var screen: modelData
                                x: layoutArea.screenItemX(screen)
                                y: layoutArea.screenItemY(screen)
                                width: 116
                                height: 78
                                scale: layoutArea.zoom

                                function restorePositionBindings() {
                                    screenTile.x = Qt.binding(function() {
                                        return layoutArea.screenItemX(screenTile.screen)
                                    })
                                    screenTile.y = Qt.binding(function() {
                                        return layoutArea.screenItemY(screenTile.screen)
                                    })
                                }

                                Rectangle {
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    height: 68
                                    radius: 5
                                    color: screenTile.screen.isLocal ? "#356ae6" : "#ffffff"
                                    border.width: layoutArea.isActiveScreen(screenTile.screen) ? 3
                                        : (layoutArea.selectedIndex === screenTile.index ? 2
                                           : (screenTile.screen.isLocal ? 0 : 1))
                                    border.color: layoutArea.isActiveScreen(screenTile.screen)
                                        ? "#2f9e62"
                                        : (layoutArea.selectedIndex === screenTile.index
                                           ? "#e09a24" : "#9ca4af")

                                    Column {
                                        anchors.fill: parent
                                        anchors.margins: 9
                                        spacing: 3
                                        Text {
                                            text: screenTile.screen.isLocal ? "本机" : "远端设备"
                                            color: screenTile.screen.isLocal ? "#dfe8ff" : "#747b85"
                                            font.pixelSize: 10
                                        }
                                        Text {
                                            width: parent.width
                                            text: screenTile.screen.ownerId
                                            color: screenTile.screen.isLocal ? "#ffffff" : "#292e36"
                                            elide: Text.ElideRight
                                            font.pixelSize: 12
                                            font.weight: Font.DemiBold
                                        }
                                        Text {
                                            text: "屏幕 " + screenTile.screen.screenIndex
                                            color: screenTile.screen.isLocal ? "#dfe8ff" : "#777e88"
                                            font.pixelSize: 10
                                        }
                                    }
                                }
                                Rectangle {
                                    visible: layoutArea.isActiveScreen(screenTile.screen)
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 6
                                    width: 8
                                    height: 8
                                    radius: 4
                                    color: "#2f9e62"
                                    border.width: 1
                                    border.color: "#ffffff"
                                }
                                Rectangle {
                                    anchors.horizontalCenter: parent.horizontalCenter
                                    anchors.bottom: parent.bottom
                                    width: 28
                                    height: 3
                                    radius: 1
                                    color: screenTile.screen.isLocal ? "#2856bc" : "#8d949e"
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    drag.target: screenTile
                                    cursorShape: pressed ? Qt.ClosedHandCursor : Qt.OpenHandCursor
                                    onPressed: layoutArea.selectedIndex = screenTile.index
                                    onReleased: {
                                        var nextX = Math.round((screenTile.x + screenTile.width / 2
                                            - layoutArea.width / 2 - layoutArea.panX)
                                            / (layoutArea.cellWidth * layoutArea.zoom))
                                        var nextY = Math.round((screenTile.y + screenTile.height / 2
                                            - layoutArea.height / 2 - layoutArea.panY)
                                            / (layoutArea.cellHeight * layoutArea.zoom))
                                        root.presenter.moveScreen(index, nextX, nextY)
                                        screenTile.restorePositionBindings()
                                    }
                                    onCanceled: screenTile.restorePositionBindings()
                                    onWheel: function(wheel) {
                                        layoutArea.applyWheel(wheel, parent)
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Components.SectionCard {
                Layout.fillWidth: root.narrowLayout
                Layout.fillHeight: !root.narrowLayout
                Layout.preferredWidth: root.narrowLayout ? 0 : 250
                Layout.minimumWidth: root.narrowLayout ? 0 : 230
                Layout.preferredHeight: root.narrowLayout ? 166 : 0
                contentMargins: 0

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.margins: 14
                        Text {
                            text: "已配置屏幕"
                            color: "#303640"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            Layout.fillWidth: true
                        }
                        Text {
                            text: root.presenter.screens.length
                            color: "#747b85"
                            font.pixelSize: 12
                        }
                    }
                    Rectangle { Layout.fillWidth: true; implicitHeight: 1; color: "#e1e4e8" }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        contentWidth: availableWidth

                        ColumnLayout {
                            width: parent.width
                            spacing: 0

                            Text {
                                visible: root.presenter.screens.length === 0
                                text: "列表为空"
                                color: "#9096a0"
                                font.pixelSize: 12
                                Layout.alignment: Qt.AlignHCenter
                                Layout.topMargin: 24
                            }

                            Repeater {
                                model: root.presenter.screens

                                delegate: ItemDelegate {
                                    id: screenListDelegate
                                    required property var modelData
                                    required property int index

                                    Layout.fillWidth: true
                                    implicitHeight: 61
                                    hoverEnabled: true
                                    onClicked: layoutArea.focusIndex(index)

                                    contentItem: RowLayout {
                                        spacing: 9
                                        Rectangle {
                                            Layout.preferredWidth: 10
                                            Layout.preferredHeight: 38
                                            radius: 2
                                            color: layoutArea.isActiveScreen(modelData) ? "#2f9e62"
                                                : (modelData.isLocal ? "#356ae6" : "#c5cad1")
                                        }
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 2
                                            Text {
                                                text: (modelData.isLocal ? "本机 · 屏幕 "
                                                    : "远端 · 屏幕 ") + modelData.screenIndex
                                                    + (layoutArea.isActiveScreen(modelData)
                                                       ? " · 当前" : "")
                                                color: "#303640"
                                                font.pixelSize: 12
                                                font.weight: Font.DemiBold
                                            }
                                            Text {
                                                text: modelData.ownerId
                                                color: "#7c838d"
                                                font.pixelSize: 10
                                                elide: Text.ElideMiddle
                                                Layout.fillWidth: true
                                            }
                                            Text {
                                                text: "位置  " + modelData.x + ", " + modelData.y
                                                color: "#8b929c"
                                                font.pixelSize: 10
                                            }
                                        }
                                        ToolButton {
                                            id: removeButton
                                            text: "×"
                                            onClicked: root.presenter.removeScreen(index)
                                            ToolTip.visible: hovered
                                            ToolTip.text: "移除屏幕"
                                            contentItem: Text {
                                                text: removeButton.text
                                                color: removeButton.hovered ? "#b33a3a" : "#838a94"
                                                font.pixelSize: 17
                                                horizontalAlignment: Text.AlignHCenter
                                                verticalAlignment: Text.AlignVCenter
                                            }
                                            background: Rectangle {
                                                radius: 3
                                                color: removeButton.hovered ? "#f9e7e7" : "transparent"
                                            }
                                        }
                                    }
                                    background: Rectangle {
                                        color: layoutArea.selectedIndex === screenListDelegate.index
                                            ? "#fff7e8"
                                            : (screenListDelegate.hovered ? "#f4f5f7" : "#ffffff")
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
