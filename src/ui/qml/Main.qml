import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls.Material 2.15
import QtQuick.Effects
import DshPluginManager

ApplicationWindow {
    id: root
    visible: true
    width: 1120
    height: 760
    minimumWidth: 860
    minimumHeight: 540
    title: "DSH 插件管理器"
    color: Theme.window

    Material.theme: Material.Dark
    Material.accent: Theme.primary
    Material.background: Theme.window
    Material.foreground: Theme.text

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // ===== 侧边栏 =====
        Rectangle {
            Layout.preferredWidth: 236
            Layout.fillHeight: true
            color: Theme.sidebar

            // 右侧 1px 分隔线
            Rectangle {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.bottom: parent.bottom
                width: 1
                color: Theme.separator
            }

            ColumnLayout {
                anchors.fill: parent
                anchors.leftMargin: 14
                anchors.rightMargin: 14
                anchors.topMargin: 18
                anchors.bottomMargin: 14
                spacing: 6

                // Logo + 标题
                RowLayout {
                    Layout.fillWidth: true
                    Layout.bottomMargin: 10
                    spacing: 10

                    // 应用图标（圆角裁切）
                    Rectangle {
                        Layout.preferredWidth: 34
                        Layout.preferredHeight: 34
                        radius: 8
                        clip: true

                        Image {
                            anchors.fill: parent
                            source: "qrc:/images/logo.png"
                            sourceSize: Qt.size(68, 68)
                            fillMode: Image.PreserveAspectCrop
                        }
                    }

                    ColumnLayout {
                        spacing: 1

                        Text {
                            text: "DSH 插件管理"
                            font.pixelSize: 14
                            font.weight: Font.DemiBold
                            color: Theme.text
                        }

                        Text {
                            text: "DeepSeek Harness"
                            font.pixelSize: Theme.fontMini
                            color: Theme.textTertiary
                        }
                    }
                }

                // Profile 区块
                Text {
                    text: "PROFILE"
                    font.pixelSize: Theme.fontMini
                    font.weight: Font.Medium
                    font.letterSpacing: 0.8
                    color: Theme.textTertiary
                    Layout.leftMargin: 6
                }

                ComboBox {
                    id: profileCombo
                    Layout.fillWidth: true
                    Layout.bottomMargin: 12
                    model: pluginManager.profiles

                    Component.onCompleted: {
                        const idx = pluginManager.profiles.indexOf(pluginManager.currentProfile)
                        if (idx >= 0) currentIndex = idx
                    }

                    onActivated: {
                        pluginManager.currentProfile = currentText
                    }
                }

                // 导航菜单（macOS 胶囊式选中）
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 2
                    currentIndex: 0
                    clip: true

                    model: [
                        { "name": "插件管理", "icon": "grid" },
                        { "name": "终端会话", "icon": "terminal" },
                        { "name": "远程服务器", "icon": "server" },
                        { "name": "设置", "icon": "gear" }
                    ]

                    delegate: Rectangle {
                        id: navItem
                        width: navList.width
                        height: 36
                        radius: 7
                        color: navList.currentIndex === index
                               ? (navMa.pressed ? Theme.primaryDim : Theme.primary)
                               : navMa.containsMouse ? "#10FFFFFF" : "transparent"

                        Behavior on color { ColorAnimation { duration: Theme.animFast } }

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: 10
                            spacing: 10

                            AppIcon {
                                width: 16
                                height: 16
                                name: modelData.icon
                                iconColor: navList.currentIndex === index
                                           ? "#FFFFFF" : Theme.textSecondary
                            }

                            Text {
                                text: modelData.name
                                font.pixelSize: Theme.fontNormal
                                font.weight: navList.currentIndex === index
                                             ? Font.Medium : Font.Normal
                                color: navList.currentIndex === index
                                       ? "#FFFFFF" : Theme.textSecondary
                            }

                            // 有可用更新时，在「设置」项上显示红点
                            Rectangle {
                                visible: modelData.icon === "gear" && updateChecker.updateAvailable
                                Layout.preferredWidth: 7
                                Layout.preferredHeight: 7
                                radius: 4
                                color: Theme.danger
                            }
                        }

                        MouseArea {
                            id: navMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                navList.currentIndex = index
                                stackLayout.currentIndex = index
                            }
                        }
                    }
                }

                // 底部版本号
                Text {
                    text: "v" + Qt.application.version + " · Qt 6"
                    font.pixelSize: Theme.fontMini
                    color: Theme.textTertiary
                    Layout.alignment: Qt.AlignHCenter
                }
            }
        }

        // ===== 主内容区域 =====
        StackLayout {
            id: stackLayout
            Layout.fillWidth: true
            Layout.fillHeight: true

            // 插件管理（页内分段筛选：全部 / 已启用）
            PluginList {
                plugins: pluginManager.plugins
                loading: pluginManager.loading
                onRefresh: pluginManager.refresh()
                onInstallRequested: installDialog.open()
                onUninstallRequested: function (id) { confirmDialog.askUninstall(id) }
                onToggleRequested: function (id, enabled) { pluginManager.togglePlugin(id, enabled) }
                onOpenDirectory: function (id) { pluginManager.openPluginDirectory(id) }
            }

            // 终端会话
            TmuxPage {
            }

            // 远程服务器
            RemotePage {
            }

            // 设置页面
            Rectangle {
                color: Theme.window

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 32
                    anchors.rightMargin: 32
                    anchors.topMargin: 24
                    spacing: 18

                    Text {
                        text: "设置"
                        font.pixelSize: Theme.fontTitle
                        font.weight: Font.DemiBold
                        color: Theme.text
                    }

                    // 信息卡片
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: infoColumn.implicitHeight + 32
                        radius: Theme.radiusLarge
                        color: Theme.card
                        border.color: Theme.cardBorder
                        border.width: 1

                        ColumnLayout {
                            id: infoColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                text: "环境信息"
                                font.pixelSize: Theme.fontHeadline
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }

                            GridLayout {
                                columns: 2
                                columnSpacing: 16
                                rowSpacing: 8
                                Layout.fillWidth: true

                                Text { text: "DSH 目录"; color: Theme.textSecondary; font.pixelSize: Theme.fontNormal }
                                Text { text: pluginManager.dshHome; color: Theme.text; font.pixelSize: Theme.fontNormal; font.family: "Menlo" }

                                Text { text: "当前 Profile"; color: Theme.textSecondary; font.pixelSize: Theme.fontNormal }
                                Text { text: pluginManager.currentProfile; color: Theme.text; font.pixelSize: Theme.fontNormal }
                            }
                        }
                    }

                    // dsh 路径卡片
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: dshColumn.implicitHeight + 32
                        radius: Theme.radiusLarge
                        color: Theme.card
                        border.color: Theme.cardBorder
                        border.width: 1

                        ColumnLayout {
                            id: dshColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                text: "dsh 命令路径"
                                font.pixelSize: Theme.fontHeadline
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 8

                                TextField {
                                    id: dshPathField
                                    Layout.fillWidth: true
                                    text: pluginManager.dshExecutable
                                    placeholderText: "留空则自动检测（$PATH / homebrew / npx 缓存）"
                                    selectByMouse: true
                                }

                                Button {
                                    text: "保存"
                                    highlighted: true
                                    enabled: dshPathField.text.trim() !== pluginManager.dshExecutable
                                    onClicked: pluginManager.setDshExecutable(dshPathField.text)
                                }

                                Button {
                                    text: "自动检测"
                                    onClicked: {
                                        dshPathField.text = ""
                                        pluginManager.setDshExecutable("")
                                    }
                                }
                            }

                            Text {
                                text: "当前生效: " + (pluginManager.dshExecutable || "未找到 dsh")
                                color: pluginManager.dshExecutable ? Theme.textSecondary : Theme.danger
                                font.pixelSize: Theme.fontSmall
                                font.family: "Menlo"
                                wrapMode: Text.WrapAnywhere
                                Layout.fillWidth: true
                            }
                        }
                    }

                    // 版本更新卡片
                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: updateColumn.implicitHeight + 32
                        radius: Theme.radiusLarge
                        color: Theme.card
                        border.color: updateChecker.updateAvailable ? "#3D5470FB" : Theme.cardBorder
                        border.width: 1

                        ColumnLayout {
                            id: updateColumn
                            anchors.left: parent.left
                            anchors.right: parent.right
                            anchors.top: parent.top
                            anchors.margins: 16
                            spacing: 10

                            Text {
                                text: "版本更新"
                                font.pixelSize: Theme.fontHeadline
                                font.weight: Font.DemiBold
                                color: Theme.text
                            }

                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12

                                Text {
                                    text: "当前版本  v" + updateChecker.currentVersion
                                    color: Theme.textSecondary
                                    font.pixelSize: Theme.fontNormal
                                }

                                // 有新版本时的徽章
                                Rectangle {
                                    visible: updateChecker.updateAvailable
                                    width: newVerText.implicitWidth + 14
                                    height: 22
                                    radius: 11
                                    color: Theme.primaryBg

                                    Text {
                                        id: newVerText
                                        anchors.centerIn: parent
                                        text: "新版本 " + updateChecker.latestVersion
                                        font.pixelSize: Theme.fontSmall
                                        font.weight: Font.Medium
                                        color: Theme.primaryHover
                                    }
                                }

                                Item { Layout.fillWidth: true }

                                Button {
                                    flat: true
                                    enabled: !updateChecker.checking
                                    onClicked: updateChecker.check(false)

                                    contentItem: RowLayout {
                                        spacing: 6
                                        AppIcon {
                                            name: "refresh"; width: 14; height: 14
                                            iconColor: Theme.textSecondary
                                        }
                                        Text {
                                            text: updateChecker.checking ? "检查中…" : "检查更新"
                                            font.pixelSize: Theme.fontNormal
                                            color: Theme.textSecondary
                                        }
                                    }
                                }

                                Button {
                                    visible: updateChecker.updateAvailable
                                    highlighted: true
                                    onClicked: updateChecker.openReleasePage()

                                    contentItem: RowLayout {
                                        spacing: 6
                                        AppIcon {
                                            name: "doc"; width: 14; height: 14
                                            iconColor: "white"
                                        }
                                        Text {
                                            text: "前往下载"
                                            font.pixelSize: Theme.fontNormal
                                            color: "white"
                                        }
                                    }
                                }
                            }

                            Text {
                                visible: updateChecker.statusText.length > 0
                                text: updateChecker.statusText
                                color: updateChecker.updateAvailable ? Theme.primaryHover : Theme.textTertiary
                                font.pixelSize: Theme.fontSmall
                            }

                            // 更新日志（有更新时显示）
                            Rectangle {
                                Layout.fillWidth: true
                                visible: updateChecker.updateAvailable && updateChecker.releaseNotes.length > 0
                                implicitHeight: Math.min(notesText.implicitHeight + 20, 160)
                                radius: Theme.radius
                                color: Theme.field

                                ScrollView {
                                    anchors.fill: parent
                                    anchors.margins: 10
                                    clip: true

                                    Text {
                                        id: notesText
                                        width: parent.width
                                        text: updateChecker.releaseNotes
                                        color: Theme.textSecondary
                                        font.pixelSize: Theme.fontSmall
                                        wrapMode: Text.WordWrap
                                    }
                                }
                            }
                        }
                    }

                    Button {
                        text: "打开 Profile 目录"
                        onClicked: pluginManager.openProfileDirectory()
                    }

                    Text {
                        text: "最近命令输出"
                        color: Theme.textSecondary
                        font.pixelSize: Theme.fontNormal
                        visible: pluginManager.lastOutput.length > 0
                    }

                    // 输出日志卡片
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        visible: pluginManager.lastOutput.length > 0
                        radius: Theme.radiusLarge
                        color: Theme.field
                        border.color: Theme.cardBorder
                        border.width: 1

                        ScrollView {
                            anchors.fill: parent
                            anchors.margins: 12
                            clip: true

                            Text {
                                text: pluginManager.lastOutput
                                color: Theme.textSecondary
                                font.pixelSize: Theme.fontSmall
                                font.family: "Menlo"
                                wrapMode: Text.WrapAnywhere
                                width: parent.width
                            }
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
