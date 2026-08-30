import QtQuick 2.15
import QtQuick.Layouts 1.15
import DshPluginManager

/**
 * 统一图标组件 —— Lucide SVG 图标（https://lucide.dev，ISC 协议）
 *
 * 由于 Qt 6.11 的 Image/VectorImage 均无公开的 currentColor 支持，
 * 采用「预烘焙着色变体」方案：tools/make_icons.py 把每个图标的
 * currentColor 替换为各主题色，生成 <name>_<tone>.svg。
 * 本组件根据 iconColor 自动映射到对应变体文件。
 *
 * 用法: AppIcon { name: "folder"; iconColor: Theme.textSecondary }
 * 图标库: folder / trash / doc / restart / close / terminal
 *         grid / gear / refresh / plus / play / search
 */
Image {
    id: root

    property string name: ""
    property color iconColor: Theme.textSecondary

    // 颜色 → 变体后缀（与 tools/make_icons.py 的 TONES 保持一致）
    readonly property var _tones: ({
        "#98989f": "secondary",   // Theme.textSecondary
        "#f5f5f7": "primary",     // Theme.text（悬停/高亮）
        "#63636b": "tertiary",    // Theme.textTertiary
        "#ffffff": "white",       // 主色底上的图标
        "#ff453a": "danger",      // Theme.danger
        "#6b84fc": "blue"         // Theme.primaryHover
    })

    readonly property string _tone: {
        const key = iconColor.toString().toLowerCase()
        return _tones[key] || "secondary"
    }

    width: 16
    height: 16
    // 布局首选尺寸绑定到显式尺寸：
    // 避免 SVG 的 sourceSize(2x) 让 implicitWidth 大于 width 导致布局错位
    Layout.preferredWidth: width
    Layout.preferredHeight: height

    source: name.length > 0 ? "qrc:/icons/" + name + "_" + _tone + ".svg" : ""
    // 2x 源尺寸保证 Retina 下清晰
    sourceSize: Qt.size(width * 2, height * 2)
    fillMode: Image.PreserveAspectFit
    smooth: true
    mipmap: true
}
