import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Effects
import DshPluginManager

/**
 * 远程服务器管理页：SSH 服务器列表 + 添加/编辑/删除/连接。
 *
 * 服务器字段：备注名 / SSH 地址 / 端口 / 认证方式（密钥|密码）/ 记住密码
 * 地址栏支持粘贴完整命令（ssh -p 6005 user@host），自动拆分端口。
 */
Rectangle {
    id: root
    color: Theme.window

    // ===== 服务器表单（添加/编辑共用）=====
    component ServerFormDialog: Dialog {
        id: formDialog
        modal: true
        anchors.centerIn: parent
        width: 460
        standardButtons: Dialog.Ok | Dialog.Cancel

        property bool isEdit: false
        property string originalName: ""

        title: isEdit ? "编辑服务器" : "添加服务器"

        function askAdd() {
            isEdit = false
            originalName = ""
            formName.text = ""
            formTarget.text = ""
            formPort.text = ""
            authKey.checked = true
            formPassword.text = ""
            rememberPwd.checked = false
            open()
        }

        function askEdit(name, target, port, auth, hasSavedPwd) {
            isEdit = true
            originalName = name
            formName.text = name
            formTarget.text = target
            formPort.text = port > 0 ? String(port) : ""
            authKey.checked = auth !== "password"
            authPwd.checked = auth === "password"
            formPassword.text = ""
            rememberPwd.checked = hasSavedPwd
            open()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text { text: "备注名"; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            TextField {
                id: formName
                Layout.fillWidth: true
                placeholderText: "例如: 生产服务器"
            }

            Text { text: "SSH 地址"; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            TextField {
                id: formTarget
                Layout.fillWidth: true
                placeholderText: "user@192.168.1.100 或别名 home（不要带 ssh 前缀）"

                // 粘贴完整命令时自动拆分端口
                onTextChanged: {
                    const parsed = remoteManager.parseTarget(text)
                    if (parsed.port > 0 && parsed.target !== text) {
                        formTarget.text = parsed.target
                        formPort.text = String(parsed.port)
                    }
                }
            }

            Text { text: "端口（留空 = 默认 22）"; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            TextField {
                id: formPort
                Layout.fillWidth: true
                placeholderText: "22"
                validator: IntValidator { bottom: 1; top: 65535 }
                inputMethodHints: Qt.ImhDigitsOnly
            }

            Text { text: "认证方式"; color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
            RowLayout {
                spacing: 16

                RadioButton {
                    id: authKey
                    text: "密钥认证"
                    checked: true
                    contentItem: Text {
                        text: authKey.text
                        color: Theme.text
                        font.pixelSize: Theme.fontNormal
                        leftPadding: authKey.indicator.width + 6
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                RadioButton {
                    id: authPwd
                    text: "密码认证"
                    contentItem: Text {
                        text: authPwd.text
                        color: Theme.text
                        font.pixelSize: Theme.fontNormal
                        leftPadding: authPwd.indicator.width + 6
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }

            // 密码区（仅密码认证时显示）
            ColumnLayout {
                visible: authPwd.checked
                spacing: 8
                Layout.fillWidth: true

                Text { text: formDialog.isEdit ? "密码（留空 = 不修改已保存的）" : "密码"
                       color: Theme.textSecondary; font.pixelSize: Theme.fontSmall }
                TextField {
                    id: formPassword
                    Layout.fillWidth: true
                    echoMode: TextInput.Password
                    placeholderText: "连接时使用的登录密码"
                }

                CheckBox {
                    id: rememberPwd
                    text: "记住密码（Base64 存储在本机，安全性有限）"
                    contentItem: Text {
                        text: rememberPwd.text
                        color: Theme.textTertiary
                        font.pixelSize: Theme.fontSmall
                        leftPadding: rememberPwd.indicator.width + 6
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }

        onOpened: formName.forceActiveFocus()

        onAccepted: {
            const port = formPort.text.trim().length > 0 ? parseInt(formPort.text.trim()) : 0
            const auth = authPwd.checked ? "password" : "key"
            if (formDialog.isEdit) {
                remoteManager.editServer(formDialog.originalName, formName.text,
                                         formTarget.text, port, auth,
                                         formPassword.text, rememberPwd.checked)
            } else {
                remoteManager.addServer(formName.text, formTarget.text, port, auth,
                                        formPassword.text, rememberPwd.checked)
            }
        }
    }

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

                RowLayout {
                    spacing: 8

                    Text {
                        text: card.server ? card.server.name : ""
                        font.pixelSize: Theme.fontHeadline
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }

                    // 密码认证徽章
                    Rectangle {
                        visible: card.server && card.server.authType === "password"
                        width: authText.implicitWidth + 12
                        height: 18
                        radius: 9
                        color: "#1EFF9F0A"

                        Text {
                            id: authText
                            anchors.centerIn: parent
                            text: "密码"
                            font.pixelSize: Theme.fontMini
                            color: Theme.warning
                        }
                    }
                }

                Text {
                    text: card.server
                          ? card.server.target + (card.server.port > 0 ? ":" + card.server.port : "")
                          : ""
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

                IconBtn {
                    icon: "pencil"
                    tip: "编辑服务器"
                    onClicked: card.editRequested()
                }

                IconBtn {
                    icon: "trash"
                    tip: "删除服务器"
                    danger: true
                    onClicked: card.removeRequested()
                }
            }
        }
    }

    // ===== 幽灵图标按钮 =====
    component IconBtn: Item {
        id: btn
        property string icon: ""
        property string tip: ""
        property bool danger: false
        signal clicked()

        implicitWidth: 30
        implicitHeight: 30

        Rectangle {
            anchors.fill: parent
            radius: Theme.radiusSmall
            color: btnMa.containsMouse ? (btn.danger ? "#26FF453A" : "#14FFFFFF") : "transparent"
            Behavior on color { ColorAnimation { duration: Theme.animFast } }
        }

        AppIcon {
            anchors.centerIn: parent
            width: 15
            height: 15
            name: btn.icon
            iconColor: btnMa.containsMouse ? (btn.danger ? Theme.danger : Theme.text) : Theme.textSecondary
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
                onClicked: serverForm.askAdd()

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

        // ===== DSH 服务管理卡片（连接远程后显示）=====
        Rectangle {
            visible: pluginManager.remoteActive
            Layout.fillWidth: true
            implicitHeight: dshCardCol.implicitHeight + 28
            radius: Theme.radiusLarge
            color: Theme.card
            border.color: Theme.cardBorder
            border.width: 1

            ColumnLayout {
                id: dshCardCol
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 14
                spacing: 10

                // 标题行：状态点 + 标题 + 操作按钮
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8

                    Rectangle {
                        width: 8
                        height: 8
                        radius: 4
                        color: !remoteDshManager.dshInstalled ? Theme.textTertiary
                               : remoteDshManager.running ? Theme.success : Theme.warning
                    }

                    Text {
                        text: "DSH 服务"
                        font.pixelSize: Theme.fontHeadline
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }

                    Text {
                        visible: remoteDshManager.busy
                        text: "处理中…"
                        font.pixelSize: Theme.fontSmall
                        color: Theme.primaryHover
                    }

                    Item { Layout.fillWidth: true }

                    Button {
                        flat: true
                        enabled: !remoteDshManager.busy
                        onClicked: remoteDshManager.refresh()
                        contentItem: RowLayout {
                            spacing: 5
                            AppIcon {
                                name: "refresh"; width: 13; height: 13
                                iconColor: Theme.textSecondary
                            }
                            Text {
                                text: "刷新"; font.pixelSize: Theme.fontSmall
                                color: Theme.textSecondary
                            }
                        }
                    }

                    Button {
                        visible: remoteDshManager.dshInstalled
                        enabled: !remoteDshManager.busy && remoteDshManager.upgradeAvailable
                        highlighted: remoteDshManager.upgradeAvailable
                        onClicked: remoteDshManager.upgrade()
                        contentItem: RowLayout {
                            spacing: 5
                            AppIcon {
                                name: "download"; width: 13; height: 13
                                iconColor: remoteDshManager.upgradeAvailable ? "white" : Theme.textTertiary
                            }
                            Text {
                                text: "升级"; font.pixelSize: Theme.fontSmall
                                color: remoteDshManager.upgradeAvailable ? "white" : Theme.textTertiary
                            }
                        }
                    }

                    Button {
                        visible: remoteDshManager.dshInstalled
                        enabled: !remoteDshManager.busy && remoteDshManager.running
                        flat: true
                        onClicked: remoteDshManager.restart()
                        contentItem: RowLayout {
                            spacing: 5
                            AppIcon {
                                name: "restart"; width: 13; height: 13
                                iconColor: remoteDshManager.running ? Theme.warning : Theme.textTertiary
                            }
                            Text {
                                text: "重启"; font.pixelSize: Theme.fontSmall
                                color: remoteDshManager.running ? Theme.warning : Theme.textTertiary
                            }
                        }
                    }
                }

                // 状态信息行
                Text {
                    Layout.fillWidth: true
                    font.pixelSize: Theme.fontNormal
                    color: Theme.textSecondary
                    text: !remoteDshManager.dshInstalled
                          ? "未检测到 DSH（服务器上未找到 dsh 命令）"
                          : ("已装 v" + remoteDshManager.dshVersion
                             + (remoteDshManager.latestVersion.length > 0
                                ? "  ·  最新 v" + remoteDshManager.latestVersion : "")
                             + (remoteDshManager.upgradeAvailable ? "  ·  有新版本可升级" : "")
                             + "  ·  " + remoteDshManager.runModeText)
                }

                // 操作日志（有内容时显示，最多 6 行高）
                Rectangle {
                    visible: remoteDshManager.log.length > 0
                    Layout.fillWidth: true
                    Layout.preferredHeight: Math.min(logText.implicitHeight + 16, 120)
                    radius: Theme.radiusSmall
                    color: Theme.field
                    clip: true

                    Flickable {
                        anchors.fill: parent
                        anchors.margins: 8
                        contentWidth: width
                        contentHeight: logText.implicitHeight
                        flickableDirection: Flickable.VerticalFlick
                        // 始终停在最底部，看最新日志
                        onContentHeightChanged: contentY = Math.max(0, contentHeight - height)

                        Text {
                            id: logText
                            width: parent.width
                            text: remoteDshManager.log
                            font.pixelSize: Theme.fontSmall
                            font.family: "Menlo"
                            color: Theme.textSecondary
                            wrapMode: Text.WrapAnywhere
                        }
                    }
                }
            }
        }

        // 连接提示
        Text {
            Layout.fillWidth: true
            text: "支持密钥认证（需 ssh-copy-id 免密）和密码认证；"
                  + "地址栏可直接粘贴完整命令（如 ssh -p 6005 user@host），端口自动拆分。"
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
                onConnectRequested: {
                    if (remoteManager.needsPassword(modelData.name)) {
                        passwordDialog.ask(modelData.name)
                    } else {
                        remoteManager.connectToServer(modelData.name, "")
                    }
                }
                onEditRequested: serverForm.askEdit(
                    modelData.name, modelData.target,
                    modelData.port || 0,
                    modelData.authType || "key",
                    !!modelData.password)
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

    // ===== 添加/编辑服务器对话框 =====
    ServerFormDialog {
        id: serverForm
    }

    // ===== 连接等待动画（模态，连接期间显示）=====
    Dialog {
        id: connectingDialog
        modal: true
        anchors.centerIn: parent
        width: 320
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.NoButton

        // 连接状态驱动显隐
        visible: remoteManager.connecting

        contentItem: ColumnLayout {
            spacing: 16

            // 旋转动画
            BusyIndicator {
                Layout.alignment: Qt.AlignHCenter
                running: connectingDialog.visible
                implicitWidth: 48
                implicitHeight: 48
            }

            Text {
                text: "正在连接服务器…"
                font.pixelSize: Theme.fontNormal
                color: Theme.text
                Layout.alignment: Qt.AlignHCenter
            }

            Text {
                text: "首次连接可能需要 5~15 秒\n（正在扫描远程插件）"
                font.pixelSize: Theme.fontSmall
                color: Theme.textTertiary
                horizontalAlignment: Text.AlignHCenter
                Layout.alignment: Qt.AlignHCenter
            }
        }
    }

    // ===== 密码输入对话框（连接时）=====
    Dialog {
        id: passwordDialog
        title: "输入密码"
        modal: true
        anchors.centerIn: parent
        width: 400
        standardButtons: Dialog.Ok | Dialog.Cancel

        property string serverName: ""

        function ask(name) {
            serverName = name
            pwdField.text = ""
            rememberConnect.checked = true
            open()
        }

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "服务器 " + passwordDialog.serverName + " 使用密码认证："
                color: Theme.text
                font.pixelSize: Theme.fontNormal
            }

            TextField {
                id: pwdField
                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: "登录密码"
            }

            CheckBox {
                id: rememberConnect
                text: "记住密码（下次免输）"
                checked: true
                contentItem: Text {
                    text: rememberConnect.text
                    color: Theme.textTertiary
                    font.pixelSize: Theme.fontSmall
                    leftPadding: rememberConnect.indicator.width + 6
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        onOpened: pwdField.forceActiveFocus()

        onAccepted: {
            // 勾选记住：先写回服务器记录，再连接（连接时使用保存的密码）
            if (rememberConnect.checked) {
                const s = remoteManager.servers.find(function (x) {
                    return x.name === passwordDialog.serverName
                })
                if (s) {
                    remoteManager.editServer(s.name, s.name, s.target,
                                             s.port || 0, s.authType || "password",
                                             pwdField.text, true)
                }
                remoteManager.connectToServer(passwordDialog.serverName, "")
            } else {
                remoteManager.connectToServer(passwordDialog.serverName, pwdField.text)
            }
        }
    }

    // 消息转发
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
