import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import DshPluginManager

Rectangle {
    id: root
    color: Theme.background

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
        anchors.margins: 24
        spacing: 16

        // 标题栏
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            Text {
                text: "插件管理"
                font.pixelSize: Theme.fontTitle
                font.bold: true
                color: Theme.text
            }

            Item { Layout.fillWidth: true }

            Button {
                text: "⟳ 刷新"
                onClicked: root.refresh()
            }

            Button {
                text: "＋ 安装插件"
                highlighted: true
                onClicked: root.installRequested()
            }
        }

        // 分段筛选：全部 / 已启用
        RowLayout {
            spacing: 0

            Repeater {
                model: [
                    { "key": "all", "label": "全部 (" + root.plugins.length + ")" },
                    { "key": "enabled", "label": "已启用 (" + root.enabledCount + ")" }
                ]

                delegate: Rectangle {
                    width: segText.implicitWidth + 28
                    height: 32
                    radius: 6
                    color: root.filterMode === modelData.key ? Theme.primary
                         : segMa.containsMouse ? Theme.surfaceHover : Theme.surface

                    Text {
                        id: segText
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: Theme.fontSmall
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

            Item { Layout.fillWidth: true }
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
            spacing: 12
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
            Text {
                anchors.centerIn: parent
                text: root.loading ? ""
                    : root.plugins.length === 0 ? "当前 Profile 暂无插件"
                    : root.filterMode === "enabled" && root.enabledCount === 0 ? "暂无已启用的插件"
                    : "没有匹配的插件"
                font.pixelSize: Theme.fontNormal
                color: Theme.textSecondary
                visible: root.filteredPlugins.length === 0 && !root.loading
            }
        }
    }
}
