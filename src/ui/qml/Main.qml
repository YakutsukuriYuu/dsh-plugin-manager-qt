import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import DshPluginManager

ApplicationWindow {
    id: root
    visible: true
    width: 1100
    height: 750
    minimumWidth: 800
    minimumHeight: 500
    title: "DSH 插件管理器"

    Material.theme: Material.Dark
    Material.accent: Material.Purple

    // 按启用状态过滤后的插件列表（QVariantList 在 QML 中是 JS 数组，支持 filter）
    function enabledPlugins() {
        return pluginManager.plugins.filter(function (p) { return p.enabled })
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ===== 侧边栏 =====
        Rectangle {
            Layout.preferredWidth: 230
            Layout.fillHeight: true
            color: Theme.surface

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 16
                spacing: 16

                Text {
                    text: "DSH 插件管理器"
                    font.pixelSize: Theme.fontLarge
                    font.bold: true
                    color: Theme.text
                    Layout.alignment: Qt.AlignHCenter
                }

                Text {
                    text: "Profile"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textSecondary
                }

                ComboBox {
                    id: profileCombo
                    Layout.fillWidth: true
                    model: pluginManager.profiles

                    Component.onCompleted: {
                        const idx = pluginManager.profiles.indexOf(pluginManager.currentProfile)
                        if (idx >= 0) currentIndex = idx
                    }

                    onActivated: {
                        pluginManager.currentProfile = currentText
                    }
                }

                // 导航菜单
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4
                    currentIndex: 0

                    model: [
                        { "name": "全部插件", "icon": "☰" },
                        { "name": "已启用", "icon": "✓" },
                        { "name": "终端会话", "icon": ">_" },
                        { "name": "设置", "icon": "⚙" }
                    ]

                    delegate: ItemDelegate {
                        width: navList.width
                        height: 44
                        highlighted: navList.currentIndex === index

                        contentItem: RowLayout {
                            spacing: 12

                            Text {
                                text: modelData.icon
                                font.pixelSize: 16
                                color: Theme.text
                            }

                            Text {
                                text: modelData.name
                                font.pixelSize: Theme.fontNormal
                                color: Theme.text
                            }
                        }

                        onClicked: {
                            navList.currentIndex = index
                            stackLayout.currentIndex = index
                        }
                    }
                }
            }
        }

        // ===== 主内容区域 =====
        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 全部插件
            PluginList {
                title: "全部插件"
                plugins: pluginManager.plugins
                loading: pluginManager.loading
                onRefresh: pluginManager.refresh()
                onInstallRequested: installDialog.open()
                onUninstallRequested: function (id) { confirmDialog.askUninstall(id) }
                onToggleRequested: function (id, enabled) { pluginManager.togglePlugin(id, enabled) }
                onOpenDirectory: function (id) { pluginManager.openPluginDirectory(id) }
            }

            // 已启用
            PluginList {
                title: "已启用的插件"
                plugins: enabledPlugins()
                loading: pluginManager.loading
                showTransitive: true
                showTransitiveToggle: false
                onRefresh: pluginManager.refresh()
                onInstallRequested: installDialog.open()
                onUninstallRequested: function (id) { confirmDialog.askUninstall(id) }
                onToggleRequested: function (id, enabled) { pluginManager.togglePlugin(id, enabled) }
                onOpenDirectory: function (id) { pluginManager.openPluginDirectory(id) }
            }

            // 终端会话
            TmuxPage {
            }

            // 设置页面
            Rectangle {
                color: Theme.background

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 32
                    spacing: 20

                    Text {
                        text: "设置"
                        font.pixelSize: Theme.fontTitle
                        font.bold: true
                        color: Theme.text
                    }

                    GridLayout {
                        columns: 2
                        columnSpacing: 16
                        rowSpacing: 12
                        Layout.fillWidth: true

                        Text { text: "DSH 目录:"; color: Theme.textSecondary; font.pixelSize: Theme.fontNormal }
                        Text { text: pluginManager.dshHome; color: Theme.text; font.pixelSize: Theme.fontNormal }

                        Text { text: "当前 Profile:"; color: Theme.textSecondary; font.pixelSize: Theme.fontNormal }
                        Text { text: pluginManager.currentProfile; color: Theme.text; font.pixelSize: Theme.fontNormal }

                        Text { text: "dsh 命令:"; color: Theme.textSecondary; font.pixelSize: Theme.fontNormal }
                        Text {
                            text: pluginManager.dshExecutable || "未找到"
                            color: pluginManager.dshExecutable ? Theme.text : Theme.danger
                            font.pixelSize: Theme.fontNormal
                            Layout.fillWidth: true
                            wrapMode: Text.WrapAnywhere
                        }
                    }

                    Button {
                        text: "打开 Profile 目录"
                        onClicked: pluginManager.openProfileDirectory()
                    }

                    Text {
                        text: "最近命令输出:"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontNormal
                        visible: pluginManager.lastOutput.length > 0
                    }

                    ScrollView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: pluginManager.lastOutput.length > 0
                        clip: true

                        Text {
                            text: pluginManager.lastOutput
                            color: Theme.textSecondary
                            font.pixelSize: Theme.fontSmall
                            font.family: "Menlo"
                            wrapMode: Text.WrapAnywhere
                            width: parent ? parent.width : 0
                        }
                    }

                    Item { Layout.fillHeight: true }
                }
            }
        }
    }

    // ===== 安装对话框 =====
    Dialog {
        id: installDialog
        title: "安装插件"
        modal: true
        anchors.centerIn: parent
        width: 420
        standardButtons: Dialog.Ok | Dialog.Cancel

        contentItem: ColumnLayout {
            spacing: 12

            Text {
                text: "输入 npm 包名或本地路径:"
                font.pixelSize: Theme.fontNormal
                color: Theme.text
            }

            TextField {
                id: packageNameField
                Layout.fillWidth: true
                placeholderText: "例如: dsh-history-jump"
            }
        }

        onOpened: packageNameField.forceActiveFocus()

        onAccepted: {
            if (packageNameField.text.trim().length > 0)
                pluginManager.installPlugin(packageNameField.text.trim())
            packageNameField.text = ""
        }

        onRejected: packageNameField.text = ""
    }

    // ===== 卸载确认对话框 =====
    Dialog {
        id: confirmDialog
        title: "确认卸载"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No

        property string pluginId: ""

        function askUninstall(id) {
            pluginId = id
            open()
        }

        contentItem: Item {
            implicitWidth: confirmText.implicitWidth
            implicitHeight: confirmText.implicitHeight

            Text {
                id: confirmText
                text: "确定要卸载插件 " + confirmDialog.pluginId + " 吗？"
                color: Theme.text
                font.pixelSize: Theme.fontNormal
            }
        }

        onAccepted: pluginManager.uninstallPlugin(pluginId)
    }

    // ===== 消息提示对话框 =====
    Dialog {
        id: messageDialog
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok

        property string message: ""
        property bool isError: false

        title: isError ? "错误" : "提示"

        contentItem: Item {
            implicitWidth: 440
            implicitHeight: msgText.implicitHeight

            Text {
                id: msgText
                width: parent.width
                text: messageDialog.message
                color: Theme.text
                font.pixelSize: Theme.fontNormal
                wrapMode: Text.WordWrap
            }
        }
    }

    Connections {
        target: pluginManager
        function onErrorOccurred(message) {
            messageDialog.isError = true
            messageDialog.message = message
            messageDialog.open()
        }
        function onOperationSucceeded(message) {
            messageDialog.isError = false
            messageDialog.message = message
            messageDialog.open()
        }
    }
}
