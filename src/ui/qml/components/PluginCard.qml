import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects
import DshPluginManager

Rectangle {
    id: root
    height: 104
    radius: Theme.radiusLarge
    color: mouseArea.containsMouse ? Theme.cardHover : Theme.card
    border.color: mouseArea.containsMouse ? "#3D5470FB" : Theme.cardBorder
    border.width: 1

    Behavior on color { ColorAnimation { duration: Theme.animFast } }
    Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

    // 柔和投影（悬停加深）
    layer.enabled: true
    layer.effect: MultiEffect {
        shadowEnabled: true
        shadowBlur: Theme.shadowBlur
        shadowColor: mouseArea.containsMouse ? Theme.shadowHoverColor : Theme.shadowColor
        shadowVerticalOffset: mouseArea.containsMouse ? Theme.shadowHoverYOff : Theme.shadowYOff
        Behavior on shadowColor { ColorAnimation { duration: Theme.animNormal } }
        Behavior on shadowVerticalOffset { NumberAnimation { duration: Theme.animNormal } }
    }

    property var plugin: null

    // 卸载/操作期间禁用交互（loading 时防重复点击）
    property bool busy: false

    // 子依赖插件弱化
    opacity: root.plugin && root.plugin.direct === false ? 0.6 : 1.0

    signal uninstallRequested()
    signal toggleRequested(bool enabled)
    signal openDirectory()

    // ===== 启用/禁用开关 =====
    component CardSwitch: Item {
        id: switchRoot
        property bool checked: false
        signal toggled(bool checked)

        implicitWidth: 42
        implicitHeight: 24

        Rectangle {
            anchors.fill: parent
            radius: height / 2
            color: switchRoot.checked ? Theme.success : "#48484C"
            Behavior on color { ColorAnimation { duration: Theme.animNormal } }
        }

        Rectangle {
            width: 18
            height: 18
            radius: 9
            y: 3
            x: switchRoot.checked ? switchRoot.width - width - 3 : 3
            color: "white"
            Behavior on x { NumberAnimation { duration: Theme.animNormal; easing.type: Easing.OutCubic } }
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

    // ===== 幽灵图标按钮 =====
    component IconButton: Item {
        id: iconRoot
        property string icon: ""
        property string tip: ""
        property color iconColor: Theme.textSecondary
        property color hoverColor: "#14FFFFFF"
        property color hoverIconColor: Theme.text
        property bool enabled: true
        signal clicked()

        implicitWidth: 30
        implicitHeight: 30
        opacity: iconRoot.enabled ? 1.0 : 0.4

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: iconMa.containsMouse ? iconRoot.hoverColor : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.animFast } }
        }

        AppIcon {
            anchors.centerIn: parent
            width: 15
            height: 15
            name: iconRoot.icon
            iconColor: iconMa.containsMouse ? iconRoot.hoverIconColor : iconRoot.iconColor
        }

        MouseArea {
            id: iconMa
            anchors.fill: parent
            enabled: iconRoot.enabled
            hoverEnabled: iconRoot.enabled
            cursorShape: iconRoot.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
            onClicked: iconRoot.clicked()
        }

        ToolTip.visible: iconMa.containsMouse && iconRoot.tip.length > 0 && iconRoot.enabled
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
        anchors.leftMargin: 16
        anchors.rightMargin: 16
        anchors.topMargin: 12
        anchors.bottomMargin: 12
        spacing: 14

        // 插件首字母图标
        Rectangle {
            Layout.preferredWidth: 48
            Layout.preferredHeight: 48
            Layout.alignment: Qt.AlignVCenter
            radius: 10
            color: Theme.primaryBg

            Text {
                anchors.centerIn: parent
                text: root.plugin && root.plugin.name ? root.plugin.name.replace(/^@[^/]+\//, "").charAt(0).toUpperCase() : "?"
                font.pixelSize: 20
                font.weight: Font.DemiBold
                color: Theme.primaryHover
            }
        }

        // 插件信息
        ColumnLayout {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            spacing: 2

            RowLayout {
                spacing: 8

                Text {
                    text: root.plugin ? root.plugin.name : ""
                    font.pixelSize: Theme.fontHeadline
                    font.weight: Font.DemiBold
                    color: Theme.text
                    elide: Text.ElideRight
                    Layout.maximumWidth: 420
                }

                Text {
                    text: root.plugin ? "v" + root.plugin.version : ""
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textTertiary
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
                spacing: 6

                // 子依赖标签
                Rectangle {
                    visible: root.plugin && root.plugin.direct === false
                    width: depText.implicitWidth + 12
                    height: 18
                    radius: 9
                    color: "transparent"
                    border.color: Theme.textTertiary
                    border.width: 1

                    Text {
                        id: depText
                        anchors.centerIn: parent
                        text: "子依赖"
                        font.pixelSize: Theme.fontMini
                        color: Theme.textTertiary
                    }
                }

                // Bundle 标签
                Rectangle {
                    visible: root.plugin && root.plugin.hasBundlePatch
                    width: bundleText.implicitWidth + 12
                    height: 18
                    radius: 9
                    color: Theme.primaryBg

                    Text {
                        id: bundleText
                        anchors.centerIn: parent
                        text: "Bundle"
                        font.pixelSize: Theme.fontMini
                        color: Theme.primaryHover
                    }
                }
            }
        }

        // ===== 操作区 =====
        ColumnLayout {
            spacing: 4
            Layout.alignment: Qt.AlignVCenter

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 8

                Text {
                    text: root.plugin && root.plugin.enabled ? "已启用" : "已禁用"
                    font.pixelSize: Theme.fontMini
                    color: root.plugin && root.plugin.enabled ? Theme.success : Theme.textTertiary
                }

                CardSwitch {
                    checked: root.plugin ? root.plugin.enabled : false
                    onToggled: function (c) { root.toggleRequested(c) }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 2

                IconButton {
                    icon: "folder"
                    tip: "打开插件目录"
                    onClicked: root.openDirectory()
                }

                IconButton {
                    icon: "trash"
                    tip: "卸载插件"
                    hoverColor: "#26FF453A"
                    hoverIconColor: Theme.danger
                    enabled: !root.busy
                    opacity: root.busy ? 0.4 : 1.0
                    onClicked: root.uninstallRequested()
                }
            }
        }
    }
}
