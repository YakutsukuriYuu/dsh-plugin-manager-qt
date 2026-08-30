import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import DshPluginManager

Rectangle {
    id: root
    height: 110
    radius: Theme.radius
    color: mouseArea.containsMouse ? Theme.surfaceHover : Theme.surface
    border.color: mouseArea.containsMouse ? Theme.primary : "transparent"
    border.width: 1

    property var plugin: null

    // 子依赖插件显示为半透明，弱化视觉效果
    opacity: root.plugin && root.plugin.direct === false ? 0.65 : 1.0

    signal uninstallRequested()
    signal toggleRequested(bool enabled)
    signal openDirectory()

    // ===== 内联组件：启用/禁用开关（带滑动动画）=====
    component CardSwitch: Item {
        id: switchRoot
        property bool checked: false
        signal toggled(bool checked)

        implicitWidth: 44
        implicitHeight: 24

        // 轨道
        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: switchRoot.checked ? Theme.success : "#555555"
            Behavior on color { ColorAnimation { duration: 150 } }
        }

        // 滑块
        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 3
            x: switchRoot.checked ? switchRoot.width - width - 3 : 3
            color: "white"
            Behavior on x { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
        }

        MouseArea {
            id: switchMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: switchRoot.toggled(!switchRoot.checked)
        }

        ToolTip.visible: switchMa.containsMouse
        ToolTip.text: switchRoot.checked ? "点击禁用" : "点击启用"
        ToolTip.delay: 600
    }

    // ===== 内联组件：幽灵图标按钮（悬停才显现背景）=====
    component IconButton: Item {
        id: iconRoot
        property string symbol: ""
        property string tip: ""
        property color symbolColor: Theme.textSecondary
        property color hoverColor: Theme.surfaceHover
        property color hoverSymbolColor: Theme.text
        signal clicked()

        implicitWidth: 32
        implicitHeight: 32

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: iconMa.containsMouse ? iconRoot.hoverColor : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        Text {
            anchors.centerIn: parent
            text: iconRoot.symbol
            font.pixelSize: 15
            color: iconMa.containsMouse ? iconRoot.hoverSymbolColor : iconRoot.symbolColor
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        MouseArea {
            id: iconMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: iconRoot.clicked()
        }

        ToolTip.visible: iconMa.containsMouse && iconRoot.tip.length > 0
        ToolTip.text: iconRoot.tip
        ToolTip.delay: 600
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 14

        // 插件图标（首字母）
        Rectangle {
            Layout.preferredWidth: 56
            Layout.preferredHeight: 56
            radius: Theme.radius
            color: Theme.primary

            Text {
                anchors.centerIn: parent
                text: root.plugin && root.plugin.name ? root.plugin.name.replace(/^@[^/]+\//, "").charAt(0).toUpperCase() : "?"
                font.pixelSize: 22
                font.bold: true
                color: "white"
            }
        }

        // 插件信息
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 3

            RowLayout {
                spacing: 8

                Text {
                    text: root.plugin ? root.plugin.name : ""
                    font.pixelSize: Theme.fontNormal
                    font.bold: true
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.maximumWidth: 400
                }

                Text {
                    text: root.plugin ? "v" + root.plugin.version : ""
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                }
            }

            Text {
                text: root.plugin ? root.plugin.description : ""
                font.pixelSize: Theme.fontSmall
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
                maximumLineCount: 2
                elide: Text.ElideRight
                Layout.fillWidth: true
            }

            RowLayout {
                spacing: 8

                // 子依赖标签
                Rectangle {
                    visible: root.plugin && root.plugin.direct === false
                    width: depText.implicitWidth + 12
                    height: 20
                    radius: 4
                    color: "transparent"
                    border.color: Theme.textSecondary
                    border.width: 1

                    Text {
                        id: depText
                        anchors.centerIn: parent
                        text: "子依赖"
                        font.pixelSize: 10
                        color: Theme.textSecondary
                    }
                }

                // Bundle 标签
                Rectangle {
                    visible: root.plugin && root.plugin.hasBundlePatch
                    width: bundleText.implicitWidth + 12
                    height: 20
                    radius: 4
                    color: Theme.info

                    Text {
                        id: bundleText
                        anchors.centerIn: parent
                        text: "Bundle"
                        font.pixelSize: 10
                        color: "white"
                    }
                }
            }
        }

        // ===== 操作区：开关 + 图标按钮 =====
        ColumnLayout {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter

            CardSwitch {
                Layout.alignment: Qt.AlignHCenter
                checked: root.plugin ? root.plugin.enabled : false
                onToggled: function (c) { root.toggleRequested(c) }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                text: root.plugin && root.plugin.enabled ? "已启用" : "已禁用"
                font.pixelSize: 10
                color: root.plugin && root.plugin.enabled ? Theme.success : Theme.textSecondary
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 4

                IconButton {
                    symbol: "📂"
                    tip: "打开插件目录"
                    onClicked: root.openDirectory()
                }

                IconButton {
                    symbol: "🗑"
                    tip: "卸载插件"
                    hoverColor: Qt.rgba(244/255, 67/255, 54/255, 0.15)
                    hoverSymbolColor: Theme.danger
                    onClicked: root.uninstallRequested()
                }
            }
        }
    }
}
