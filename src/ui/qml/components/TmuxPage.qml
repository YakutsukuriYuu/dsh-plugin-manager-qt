import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import DshPluginManager

Rectangle {
    id: root
    color: Theme.background

    // ===== 会话卡片 =====
    component SessionCard: Rectangle {
        id: card
        property var session: null

        signal viewOutput()
        signal attach()
        signal restart()
        signal kill()

        height: 96
        radius: Theme.radius
        color: cardMa.containsMouse ? Theme.surfaceHover : Theme.surface
        border.color: session && session.isDsh ? Theme.primary : "transparent"
        border.width: 1

        MouseArea {
            id: cardMa
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
        }

        RowLayout {
            anchors.fill: parent
            anchors.margins: 14
            spacing: 14

            // 终端图标
            Rectangle {
                Layout.preferredWidth: 48
                Layout.preferredHeight: 48
                radius: Theme.radius
                color: session && session.isDsh ? Theme.primary : "#404040"

                Text {
                    anchors.centerIn: parent
                    text: ">_"
                    font.pixelSize: 18
                    font.bold: true
                    font.family: "Menlo"
                    color: "white"
                }
            }

            // 会话信息
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3

                RowLayout {
                    spacing: 8

                    Text {
                        text: card.session ? card.session.name : ""
                        font.pixelSize: Theme.fontNormal
                        font.bold: true
                        color: Theme.text
                    }

                    // DSH 标签
                    Rectangle {
                        visible: card.session && card.session.isDsh
                        width: dshText.implicitWidth + 12
                        height: 20
                        radius: 4
                        color: Theme.primary

                        Text {
                            id: dshText
                            anchors.centerIn: parent
                            text: "DSH"
                            font.pixelSize: 10
                            font.bold: true
                            color: "white"
                        }
                    }

                    // 已附着标签
                    Rectangle {
                        visible: card.session && card.session.attached
                        width: attachText.implicitWidth + 12
                        height: 20
                        radius: 4
                        color: Theme.success

                        Text {
                            id: attachText
                            anchors.centerIn: parent
                            text: "已连接"
                            font.pixelSize: 10
                            color: "white"
                        }
                    }
                }

                Text {
                    text: card.session
                          ? "%1 个窗口 · 创建于 %2 · %3 @ %4"
                            .arg(card.session.windows)
                            .arg(card.session.created)
                            .arg(card.session.command || "?")
                            .arg(card.session.workdir || "?")
                          : ""
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                    elide: Text.ElideMiddle
                    Layout.fillWidth: true
                }
            }

            // 操作按钮（幽灵图标风格，与插件卡片一致）
            RowLayout {
                spacing: 4

                GhostButton {
                    icon: "doc"
                    tip: "查看输出"
                    onClicked: card.viewOutput()
                }

                GhostButton {
                    icon: "terminal"
                    tip: "附加到终端"
                    onClicked: card.attach()
                }

                GhostButton {
                    visible: card.session && card.session.isDsh
                    icon: "restart"
                    tip: "重启 DSH"
                    onClicked: card.restart()
                }

                GhostButton {
                    icon: "close"
                    tip: "关闭会话"
                    hoverColor: Qt.rgba(244/255, 67/255, 54/255, 0.15)
                    hoverIconColor: Theme.danger
                    onClicked: card.kill()
                }
            }
        }
    }

    // ===== 幽灵图标按钮（矢量线条图标，悬停显现背景）=====
    component GhostButton: Item {
        id: btn
        property string icon: ""
        property string tip: ""
        property color iconColor: Theme.textSecondary
        property color hoverColor: Theme.surfaceHover
        property color hoverIconColor: Theme.text
        signal clicked()

        implicitWidth: 32
        implicitHeight: 32

        Rectangle {
            anchors.fill: parent
            radius: 6
            color: btnMa.containsMouse ? btn.hoverColor : "transparent"
            Behavior on color { ColorAnimation { duration: 120 } }
        }

        AppIcon {
            anchors.centerIn: parent
            name: btn.icon
            iconColor: btnMa.containsMouse ? btn.hoverIconColor : btn.iconColor
            Behavior on iconColor { ColorAnimation { duration: 120 } }
        }

        MouseArea {
            id: btnMa
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: btn.clicked()
        }

        ToolTip.visible: btnMa.containsMouse && btn.tip.length > 0
        ToolTip.text: btn.tip
        ToolTip.delay: 600
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 24
        spacing: 16

        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "终端会话"
                font.pixelSize: Theme.fontTitle
                font.bold: true
                color: Theme.text
            }

            Text {
                text: "(" + tmuxManager.sessions.length + ")"
                font.pixelSize: Theme.fontNormal
                color: Theme.textSecondary
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "⟳ 刷新"
                onClicked: tmuxManager.refresh()
            }

            Button {
                text: "＋ 新建会话"
                highlighted: true
                onClicked: newSessionDialog.open()
            }
        }

        // tmux 不可用提示
        Rectangle {
            Layout.fillWidth: true
            visible: !tmuxManager.available
            implicitHeight: warnText.implicitHeight + 24
            radius: Theme.radius
            color: Qt.rgba(255/255, 152/255, 0, 0.12)
            border.color: Theme.warning
            border.width: 1

            Text {
                id: warnText
                anchors.centerIn: parent
                width: parent.width - 24
                text: "未找到 tmux。请先安装: brew install tmux"
                color: Theme.warning
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.WordWrap
            }
        }

        // DSH 未运行横幅：一键启动
        Rectangle {
            Layout.fillWidth: true
            visible: tmuxManager.available && tmuxManager.dshSession.length === 0
            implicitHeight: dshBannerRow.implicitHeight + 24
            radius: Theme.radius
            color: Qt.rgba(124/255, 77/255, 255/255, 0.12)
            border.color: Theme.primary
            border.width: 1

            RowLayout {
                id: dshBannerRow
                anchors.centerIn: parent
                width: parent.width - 24
                spacing: 12

                Text {
                    text: "未检测到运行中的 DSH 会话"
                    color: Theme.text
                    font.pixelSize: Theme.fontNormal
                    Layout.fillWidth: true
                }

                Button {
                    text: "▶ 启动 DSH"
                    highlighted: true
                    onClicked: tmuxManager.createSession(
                                   "harness", "dsh web",
                                   pluginManager.dshHome + "/profiles/web")
                }
            }
        }

        // 会话列表
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: tmuxManager.sessions
            spacing: 12
            clip: true

            delegate: SessionCard {
                width: ListView.view.width
                session: modelData

                onViewOutput: {
                    outputDialog.sessionName = modelData.name
                    outputDialog.reload()
                    outputDialog.open()
                }
                onAttach: tmuxManager.attachSession(modelData.name)
                onRestart: restartDialog.ask(modelData.name)
                onKill: killDialog.ask(modelData.name)
            }

            Text {
                anchors.centerIn: parent
                text: "暂无 tmux 会话"
                font.pixelSize: Theme.fontNormal
                color: Theme.textSecondary
                visible: tmuxManager.sessions.length === 0 && tmuxManager.available
            }
        }
    }

    // 自动刷新（仅当前页面可见时）
    Timer {
        interval: 4000
        repeat: true
        running: root.visible && tmuxManager.available
        onTriggered: tmuxManager.refresh()
    }

    // ===== 输出查看对话框 =====
    Dialog {
        id: outputDialog
        title: "会话输出: " + sessionName
        modal: true
        anchors.centerIn: parent
        width: Math.min(root.width - 80, 800)
        height: Math.min(root.height - 120, 560)
        standardButtons: Dialog.Close

        property string sessionName: ""

        function reload() {
            outputText.text = tmuxManager.sessionOutput(sessionName, 100)
        }

        contentItem: ColumnLayout {
            spacing: 8

            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true

                Text {
                    id: outputText
                    width: outputDialog.width - 60
                    font.family: "Menlo"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.text
                    wrapMode: Text.WrapAnywhere
                    textFormat: Text.PlainText
                }
            }

            Button {
                text: "⟳ 重新加载"
                Layout.alignment: Qt.AlignRight
                onClicked: outputDialog.reload()
            }
        }
    }

    // ===== 新建会话对话框 =====
    Dialog {
        id: newSessionDialog
        title: "新建会话"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 12

            Text { text: "会话名:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: sessionNameField
                Layout.fillWidth: true
                placeholderText: "例如: harness"
            }

            Text { text: "启动命令:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: sessionCmdField
                Layout.fillWidth: true
                text: "dsh web"
            }

            Text { text: "工作目录:"; color: Theme.text; font.pixelSize: Theme.fontNormal }
            TextField {
                id: sessionDirField
                Layout.fillWidth: true
                text: pluginManager.dshHome + "/profiles/web"
            }
        }

        onAccepted: {
            if (sessionNameField.text.trim().length > 0)
                tmuxManager.createSession(sessionNameField.text.trim(),
                                          sessionCmdField.text,
                                          sessionDirField.text)
        }
    }

    // ===== 重启确认 =====
    Dialog {
        id: restartDialog
        title: "重启会话"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No

        property string sessionName: ""

        function ask(name) {
            sessionName = name
            open()
        }

        contentItem: Item {
            implicitWidth: restartText.implicitWidth
            implicitHeight: restartText.implicitHeight

            Text {
                id: restartText
                text: "重启会话 " + restartDialog.sessionName + " 中的进程？\n(将发送 Ctrl-C 后重新执行 dsh web)"
                color: Theme.text
                font.pixelSize: Theme.fontNormal
            }
        }

        onAccepted: tmuxManager.restartSession(sessionName, "dsh web")
    }

    // ===== 关闭确认 =====
    Dialog {
        id: killDialog
        title: "关闭会话"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No

        property string sessionName: ""

        function ask(name) {
            sessionName = name
            open()
        }

        contentItem: Item {
            implicitWidth: killText.implicitWidth
            implicitHeight: killText.implicitHeight

            Text {
                id: killText
                text: "确定关闭会话 " + killDialog.sessionName + " 吗？\n其中的进程将被终止。"
                color: Theme.text
                font.pixelSize: Theme.fontNormal
            }
        }

        onAccepted: tmuxManager.killSession(sessionName)
    }

    // ===== 消息提示（本页独立，QML id 不跨文件可见）=====
    Dialog {
        id: tmuxMessageDialog
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        property string message: ""
        property bool isError: false

        title: isError ? "错误" : "提示"

        contentItem: Item {
            implicitWidth: 440
            implicitHeight: tmuxMsgText.implicitHeight

            Text {
                id: tmuxMsgText
                width: parent.width
                text: tmuxMessageDialog.message
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.WordWrap
            }
        }
    }

    Connections {
        target: tmuxManager
        function onErrorOccurred(message) {
            tmuxMessageDialog.isError = true
            tmuxMessageDialog.message = message
            tmuxMessageDialog.open()
        }
        function onOperationSucceeded(message) {
            tmuxMessageDialog.isError = false
            tmuxMessageDialog.message = message
            tmuxMessageDialog.open()
        }
    }
}
