import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import DshPluginManager

/**
 * 同步对话框：本地 → 远程 插件同步。
 * 打开时构建同步计划（本地为主），勾选要同步的插件后执行。
 */
Dialog {
    id: root
    title: "同步到服务器"
    modal: true
    anchors.centerIn: parent
    width: 620
    height: 560
    standardButtons: Dialog.NoButton
    closePolicy: syncManager.syncing ? Popup.NoAutoClose : Popup.CloseOnEscape

    // 勾选的插件名集合
    property var checked: ({})

    function openSync() {
        checked = {}
        const err = syncManager.buildPlan()
        if (err.length > 0) {
            remoteMsg.isError = true
            remoteMsg.message = err
            remoteMsg.open()
            return
        }
        // 默认勾选：直接安装且需要安装或更新的（子依赖会自动随父插件带上）
        const c = {}
        for (const item of syncManager.plan) {
            c[item.name] = item.direct && item.action !== "same"
        }
        checked = c
        open()
    }

    // 选中数量
    readonly property int checkedCount: {
        let n = 0
        for (const k in checked) if (checked[k]) ++n
        return n
    }

    contentItem: ColumnLayout {
        spacing: 12

        // 说明
        Text {
            text: "本地 → " + pluginManager.backendName + "（版本以本地为准）"
            font.pixelSize: Theme.fontSmall
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        // 插件清单
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            radius: Theme.radius
            color: Theme.field
            border.color: Theme.separator
            border.width: 1

            ListView {
                anchors.fill: parent
                anchors.margins: 6
                model: syncManager.plan
                spacing: 2
                clip: true
                enabled: !syncManager.syncing

                delegate: Rectangle {
                    width: ListView.view.width
                    height: 52
                    radius: Theme.radiusSmall
                    color: rowMa.containsMouse ? "#0AFFFFFF" : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 10
                        anchors.rightMargin: 10
                        spacing: 10

                        CheckBox {
                            checked: root.checked[modelData.name] || false
                            onToggled: {
                                const c = root.checked
                                c[modelData.name] = checked
                                root.checked = c
                                root.checkedChanged()
                            }
                        }

                        ColumnLayout {
                            Layout.fillWidth: true
                            spacing: 1

                            Text {
                                text: modelData.name
                                font.pixelSize: Theme.fontNormal
                                color: Theme.text
                                elide: Text.ElideRight
                                Layout.fillWidth: true
                            }

                            Text {
                                text: "本地 v" + modelData.localVersion
                                      + (modelData.remoteVersion
                                         ? "  →  远程 v" + modelData.remoteVersion
                                         : "  →  远程未安装")
                                      + (modelData.direct ? "" : "  ·  子依赖")
                                font.pixelSize: Theme.fontSmall
                                color: Theme.textTertiary
                            }
                        }

                        // 状态徽章
                        Rectangle {
                            width: badgeText.implicitWidth + 12
                            height: 20
                            radius: 10
                            color: modelData.action === "install" ? "#1E30D158"
                                 : modelData.action === "update" ? Theme.primaryBg
                                 : "#14FFFFFF"

                            Text {
                                id: badgeText
                                anchors.centerIn: parent
                                text: modelData.action === "install" ? "新装"
                                    : modelData.action === "update" ? "更新"
                                    : "一致"
                                font.pixelSize: Theme.fontMini
                                color: modelData.action === "install" ? Theme.success
                                     : modelData.action === "update" ? Theme.primaryHover
                                     : Theme.textTertiary
                            }
                        }
                    }

                    MouseArea {
                        id: rowMa
                        anchors.fill: parent
                        hoverEnabled: true
                        propagateComposedEvents: true
                        onClicked: function(mouse) { mouse.accepted = false }
                    }
                }

                Text {
                    anchors.centerIn: parent
                    visible: syncManager.plan.length === 0
                    text: "本地没有可同步的插件"
                    color: Theme.textSecondary
                    font.pixelSize: Theme.fontNormal
                }
            }
        }

        // 进度区（同步时显示）
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 6
            visible: syncManager.syncing

            RowLayout {
                Text {
                    text: "正在同步: " + syncManager.currentPlugin
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
                Text {
                    text: syncManager.progress + "%"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                }
            }

            ProgressBar {
                Layout.fillWidth: true
                value: syncManager.progress / 100
            }
        }

        // 同步日志
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 110
            visible: logArea.text.length > 0
            radius: Theme.radiusSmall
            color: Theme.field
            border.color: Theme.separator
            border.width: 1

            ScrollView {
                anchors.fill: parent
                anchors.margins: 8
                clip: true

                Text {
                    id: logArea
                    width: parent.width
                    font.family: "Menlo"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                    wrapMode: Text.WrapAnywhere
                }
            }
        }

        // 底部按钮
        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            Button {
                text: "全选"
                flat: true
                enabled: !syncManager.syncing
                onClicked: {
                    const c = {}
                    for (const item of syncManager.plan) c[item.name] = true
                    root.checked = c
                    root.checkedChanged()
                }
            }

            Button {
                text: "仅选需更新"
                flat: true
                enabled: !syncManager.syncing
                onClicked: {
                    const c = {}
                    for (const item of syncManager.plan) c[item.name] = item.direct && item.action !== "same"
                    root.checked = c
                    root.checkedChanged()
                }
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "关闭"
                enabled: !syncManager.syncing
                onClicked: root.close()
            }

            Button {
                highlighted: true
                enabled: !syncManager.syncing && root.checkedCount > 0
                onClicked: {
                    const names = []
                    for (const k in root.checked)
                        if (root.checked[k]) names.push(k)
                    syncManager.startSync(names)
                }

                contentItem: RowLayout {
                    spacing: 6
                    AppIcon {
                        name: "upload"; width: 14; height: 14
                        iconColor: "white"
                    }
                    Text {
                        text: "开始同步 (" + root.checkedCount + ")"
                        font.pixelSize: Theme.fontNormal
                        color: "white"
                    }
                }
            }
        }
    }

    // 同步日志收集
    Connections {
        target: syncManager
        function onSyncLog(line) {
            logArea.text += line + "\n"
        }
        function onSyncFinished(ok, summary) {
            logArea.text += "\n" + summary + "\n"
            // 刷新远程插件列表
            pluginManager.refresh()
        }
    }

    // 计划构建失败提示
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
