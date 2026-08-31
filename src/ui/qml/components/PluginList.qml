import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import DshPluginManager

Rectangle {
    id: root
    color: Theme.window

    property var plugins: []
    property bool loading: false
    // 是否显示子依赖插件（被其他插件的 dependencies 带进来的包）
    property bool showTransitive: false
    // 筛选模式: "all" | "enabled"
    property string filterMode: "all"

    signal refresh()
    signal installRequested()
    signal uninstallRequested(string pluginId)
    signal toggleRequested(string pluginId, bool enabled)
    signal openDirectory(string pluginId)

    // 子依赖数量（基于直接/间接标记，不含筛选模式影响）
    readonly property int transitiveCount: {
        let n = 0
        for (let i = 0; i < plugins.length; ++i)
            if (!plugins[i].direct) ++n
        return n
    }

    // 已启用数量
    readonly property int enabledCount: {
        let n = 0
        for (let i = 0; i < plugins.length; ++i)
            if (plugins[i].enabled) ++n
        return n
    }

    // 第一层：按筛选模式过滤（全部 / 已启用）
    property var modeFiltered: {
        if (root.filterMode === "enabled")
            return plugins.filter(function (p) { return p.enabled })
        return plugins
    }

    // 第二层：按「直接安装/子依赖」过滤
    property var visiblePlugins: {
        if (root.showTransitive) return modeFiltered
        return modeFiltered.filter(function (p) { return p.direct })
    }

    // 第三层：搜索过滤
    property var filteredPlugins: {
        const kw = searchField.text.trim().toLowerCase()
        if (kw.length === 0) return visiblePlugins
        return visiblePlugins.filter(function (p) {
            return p.name.toLowerCase().indexOf(kw) >= 0
                || (p.description && p.description.toLowerCase().indexOf(kw) >= 0)
        })
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
                text: "插件管理"
                font.pixelSize: Theme.fontTitle
                font.weight: Font.DemiBold
                color: Theme.text
            }

            Item { Layout.fillWidth: true }

            // 带图标的按钮
            Button {
                flat: true
                onClicked: root.refresh()

                contentItem: RowLayout {
                    spacing: 6
                    AppIcon {
                        name: "refresh"; width: 14; height: 14
                        iconColor: Theme.textSecondary
                    }
                    Text { text: "刷新"; font.pixelSize: Theme.fontNormal; color: Theme.textSecondary }
                }
            }

            Button {
                highlighted: true
                onClicked: root.installRequested()

                contentItem: RowLayout {
                    spacing: 6
                    AppIcon {
                        name: "plus"; width: 14; height: 14
                        iconColor: "white"
                    }
                    Text { text: "安装插件"; font.pixelSize: Theme.fontNormal; color: "white" }
                }
            }
        }

        // 远程模式横幅（仅在管理远程服务器时显示）
        Rectangle {
            Layout.fillWidth: true
            visible: pluginManager.remoteActive
            implicitHeight: remoteBannerRow.implicitHeight + 20
            radius: Theme.radiusLarge
            color: Theme.primaryBg
            border.color: "#3D5470FB"
            border.width: 1

            RowLayout {
                id: remoteBannerRow
                anchors.centerIn: parent
                width: parent.width - 24
                spacing: 10

                AppIcon {
                    width: 15
                    height: 15
                    name: "server"
                    iconColor: Theme.primaryHover
                }

                Text {
                    text: "正在管理远程: " + pluginManager.backendName
                    color: Theme.text
                    font.pixelSize: Theme.fontSmall
                    Layout.fillWidth: true
                    elide: Text.ElideMiddle
                }
            }
        }

        // 分段筛选（macOS 胶囊容器风格）：全部 / 已启用
        Rectangle {
            Layout.preferredWidth: segRow.implicitWidth + 6
            Layout.preferredHeight: 34
            radius: 8
            color: Theme.field
            border.color: Theme.separator
            border.width: 1

            RowLayout {
                id: segRow
                anchors.centerIn: parent
                spacing: 2

                Repeater {
                    model: [
                        { "key": "all", "label": "全部  " + root.plugins.length },
                        { "key": "enabled", "label": "已启用  " + root.enabledCount }
                    ]

                    delegate: Rectangle {
                        Layout.preferredWidth: segText.implicitWidth + 24
                        Layout.preferredHeight: 28
                        radius: 6
                        color: root.filterMode === modelData.key ? Theme.primary
                             : segMa.containsMouse ? "#0AFFFFFF" : "transparent"

                        Behavior on color { ColorAnimation { duration: Theme.animFast } }

                        Text {
                            id: segText
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: Theme.fontSmall
                            font.weight: root.filterMode === modelData.key ? Font.Medium : Font.Normal
                            color: root.filterMode === modelData.key ? "white" : Theme.textSecondary
                        }

                        MouseArea {
                            id: segMa
                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.filterMode = modelData.key
                        }
                    }
                }
            }
        }

        // 搜索框
        TextField {
            id: searchField
            Layout.fillWidth: true
            placeholderText: "搜索插件名称或描述..."
        }

        // 子依赖开关
        CheckBox {
            visible: root.transitiveCount > 0
            text: "显示子依赖插件 (" + root.transitiveCount + " 个，由其他插件自动安装)"
            checked: root.showTransitive
            onToggled: root.showTransitive = checked

            contentItem: Text {
                text: parent.text
                font.pixelSize: Theme.fontSmall
                color: Theme.textSecondary
                leftPadding: parent.indicator.width + 6
                verticalAlignment: Text.AlignVCenter
            }
        }

        // 加载指示器
        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            visible: root.loading
            running: root.loading
        }

        // 插件列表
        ListView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: root.filteredPlugins
            spacing: 10
            clip: true

            delegate: PluginCard {
                width: ListView.view.width
                plugin: modelData

                onUninstallRequested: root.uninstallRequested(modelData.id)
                onToggleRequested: function (enabled) {
                    root.toggleRequested(modelData.id, enabled)
                }
                onOpenDirectory: root.openDirectory(modelData.id)
            }

            // 空状态提示
            ColumnLayout {
                anchors.centerIn: parent
                spacing: 10
                visible: root.filteredPlugins.length === 0 && !root.loading

                AppIcon {
                    Layout.alignment: Qt.AlignHCenter
                    width: 40
                    height: 40
                    name: "grid"
                    iconColor: Theme.textTertiary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: root.plugins.length === 0 ? "当前 Profile 暂无插件"
                        : root.filterMode === "enabled" && root.enabledCount === 0 ? "暂无已启用的插件"
                        : "没有匹配的插件"
                    font.pixelSize: Theme.fontNormal
                    color: Theme.textSecondary
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: root.plugins.length === 0
                    text: "点击右上角「安装插件」开始使用"
                    font.pixelSize: Theme.fontSmall
                    color: Theme.textTertiary
                }
            }
        }
    }
}
