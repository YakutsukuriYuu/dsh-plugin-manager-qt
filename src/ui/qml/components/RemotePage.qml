import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects
import DshPluginManager

/**
 * 远程服务器管理页：SSH 服务器列表 + 添加/删除/连接。
 * 连接成功后，「插件管理」页自动切换为远程模式。
 */
Rectangle {
    id: root
    color: Theme.window

    // ===== 服务器卡片 =====
    component ServerCard: Rectangle {
        id: card
        property var server: null

        signal connectRequested()
        signal editRequested()
        signal removeRequested()

        height: 76
        radius: Theme.radiusLarge
        color: cardMa.containsMouse ? Theme.cardHover : Theme.card
        border.color: pluginManager.remoteActive && pluginManager.backendName.indexOf(card.server ? card.server.name : "###") >= 0
                      ? "#3D5470FB" : Theme.cardBorder
        border.width: 1

        Behavior on color { ColorAnimation { duration: Theme.animFast } }
        Behavior on border.color { ColorAnimation { duration: Theme.animFast } }

        layer.enabled: true
        layer.effect: MultiEffect {
            shadowEnabled: true
            shadowBlur: Theme.shadowBlur
            shadowColor: cardMa.containsMouse ? Theme.shadowHoverColor : Theme.shadowColor
            shadowVerticalOffset: cardMa.containsMouse ? Theme.shadowHoverYOff : Theme.shadowYOff
        }

        MouseArea {
            id: cardMa
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            spacing: 14

            Rectangle {
                Layout.preferredWidth: 44
                Layout.preferredHeight: 44
                Layout.alignment: Qt.AlignVCenter
                radius: 10
                color: Theme.primaryBg

                AppIcon {
                    anchors.centerIn: parent
                    width: 20
                    height: 20
                    name: "server"
                    iconColor: Theme.primaryHover
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                Layout.alignment: Qt.AlignVCenter
                spacing: 2

                Text {
                    text: card.server ? card.server.name : ""
                    font.pixelSize: Theme.fontHeadline
                    font.weight: Font.DemiBold
                    color: Theme.text
                }

                Text {
                    text: card.server ? card.server.target : ""
                    font.pixelSize: Theme.fontSmall
                    font.family: "Menlo"
                    color: Theme.textSecondary
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }

            RowLayout {
                spacing: 8
                Layout.alignment: Qt.AlignVCenter

                Button {
                    highlighted: true
                    enabled: !remoteManager.connecting
                    onClicked: card.connectRequested()

                    contentItem: RowLayout {
                        spacing: 6
                        AppIcon {
                            name: "server"; width: 14; height: 14
                            iconColor: "white"
                        }
                        Text {
                            text: remoteManager.connecting ? "连接中…" : "连接"
                            font.pixelSize: Theme.fontNormal
                            color: "white"
                        }
                    }
                }

                // 编辑按钮
                Item {
                    implicitWidth: 30
                    implicitHeight: 30

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: editMa.containsMouse ? "#14FFFFFF" : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                    }

                    AppIcon {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        name: "pencil"
                        iconColor: editMa.containsMouse ? Theme.text : Theme.textSecondary
                    }

                    MouseArea {
                        id: editMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: card.editRequested()
                    }

                    ToolTip.visible: editMa.containsMouse
                    ToolTip.text: "编辑服务器"
                    ToolTip.delay: 600
                }

                // 删除按钮
                Item {
                    implicitWidth: 30
                    implicitHeight: 30

                    Rectangle {
                        anchors.fill: parent
                        radius: Theme.radiusSmall
                        color: delMa.containsMouse ? "#26FF453A" : "transparent"
                        Behavior on color { ColorAnimation { duration: Theme.animFast } }
                    }

                    AppIcon {
                        anchors.centerIn: parent
                        width: 15
                        height: 15
                        name: "trash"
                        iconColor: delMa.containsMouse ? Theme.danger : Theme.textSecondary
                    }

                    MouseArea {
                        id: delMa
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: card.removeRequested()
                    }

                    ToolTip.visible: delMa.containsMouse
                    ToolTip.text: "删除服务器"
                    ToolTip.delay: 600
                }
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: 28
        anchors.rightMargin: 28
        anchors.topMargin: 24
        anchors.bottomMargin: 20
        spacing: 14

        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "远程服务器"
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                color: Theme.text
            }

            Item { Layout.fillWidth: true }

            Button {
                highlighted: true
                onClicked: addDialog.open()

                contentItem: RowLayout {
                    spacing: 6
                    AppIcon {
                        name: "plus"; width: 14; height: 14
                        iconColor: "white"
                    }
                    Text { text: "添加服务器"; font.pixelSize: Theme.fontNormal; color: "white" }
                }
            }
        }

        // 当前模式横幅
        Rectangle {
            Layout.fillWidth: true
            implicitHeight: modeRow.implicitHeight + 24
            radius: Theme.radiusLarge
            color: pluginManager.remoteActive ? Theme.primaryBg : Theme.card
            border.color: pluginManager.remoteActive ? "#3D5470FB" : Theme.cardBorder
            border.width: 1

            RowLayout {
                id: modeRow
                anchors.centerIn: parent
                width: parent.width - 28
                spacing: 10

                AppIcon {
                    width: 16
                    height: 16
                    name: pluginManager.remoteActive ? "server" : "local"
                    iconColor: pluginManager.remoteActive ? Theme.primaryHover : Theme.textSecondary
                }

                Text {
                    text: pluginManager.remoteActive
                          ? "正在管理远程: " + pluginManager.backendName
                          : "当前管理目标: 本机"
                    color: Theme.text
                    font.pixelSize: Theme.fontNormal
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }

                Button {
                    visible: pluginManager.remoteActive
                    flat: true
                    onClicked: remoteManager.disconnectRemote()

                    contentItem: RowLayout {
                        spacing: 6
                        AppIcon {
                            name: "disconnect"; width: 14; height: 14
                            iconColor: Theme.warning
                        }
                        Text {
                            text: "断开，回本机"
                            font.pixelSize: Theme.fontNormal
                            color: Theme.warning
                        }
                    }
                }
            }
        }

        // 连接提示
        Text {
            Layout.fillWidth: true
            text: "提示：需要密钥认证（BatchMode 不支持密码）。请先用 ssh-copy-id user@host 配置免密登录；"
                  + "也支持 ~/.ssh/config 中配置的 Host 别名。"
            font.pixelSize: Theme.fontSmall
            color: Theme.textTertiary
            wrapMode: Text.WordWrap
        }

        // 服务器列表
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: remoteManager.servers
            spacing: 10
            clip: true

            delegate: ServerCard {
                width: ListView.view.width
                server: modelData
                onConnectRequested: remoteManager.connectToServer(modelData.name)
                onEditRequested: editDialog.ask(modelData.name, modelData.target)
                onRemoveRequested: remoteManager.removeServer(modelData.name)
            }

            // 空状态
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                visible: remoteManager.servers.length === 0

                AppIcon {
                    Layout.alignment: Qt.AlignHCenter
                    width: 40
                    height: 40
                    name: "server"
                    iconColor: Theme.textTertiary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "还没有服务器"
                    font.pixelSize: Theme.fontNormal
                    color: Theme.textSecondary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "点击右上角「添加服务器」，输入 user@host 即可"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textTertiary
                }
            }
        }
    }

    // ===== 添加服务器对话框 =====
    Dialog {
        id: addDialog
        title: "添加服务器"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 12

            Text { text: "备注名:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: serverNameField
                Layout.fillWidth: true
                placeholderText: "例如: 生产服务器"
            }

            Text { text: "SSH 地址:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: serverTargetField
                Layout.fillWidth: true
                placeholderText: "user@192.168.1.100 或 ssh config 别名"
            }
        }

        onOpened: serverNameField.forceActiveFocus()

        onAccepted: {
            remoteManager.addServer(serverNameField.text, serverTargetField.text)
            serverNameField.text = ""
            serverTargetField.text = ""
        }

        onRejected: {
            serverNameField.text = ""
            serverTargetField.text = ""
        }
    }

    // ===== 编辑服务器对话框 =====
    Dialog {
        id: editDialog
        title: "编辑服务器"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string originalName: ""

        function ask(name, target) {
            originalName = name
            editNameField.text = name
            editTargetField.text = target
            open()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text { text: "备注名:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: editNameField
                Layout.fillWidth: true
            }

            Text { text: "SSH 地址:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: editTargetField
                Layout.fillWidth: true
                placeholderText: "user@192.168.1.100 或 ssh config 别名"
            }
        }

        onOpened: editNameField.forceActiveFocus()

        onAccepted: {
            remoteManager.editServer(editDialog.originalName,
                                     editNameField.text, editTargetField.text)
        }
    }

    // 消息转发（RemoteManager 的错误/成功 → 本页弹窗）
    Connections {
        target: remoteManager
        function onErrorOccurred(message) {
            remoteMsg.isError = true
            remoteMsg.message = message
            remoteMsg.open()
        }
        function onOperationSucceeded(message) {
            remoteMsg.isError = false
            remoteMsg.message = message
            remoteMsg.open()
        }
    }

    Dialog {
        id: remoteMsg
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok
        property string message: ""
        property bool isError: false
        title: isError ? "错误" : "提示"

        contentItem: Item {
            implicitWidth: 440
            implicitHeight: remoteMsgText.implicitHeight

            Text {
                id: remoteMsgText
                width: parent.width
                text: remoteMsg.message
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.WordWrap
            }
        }
    }
}
