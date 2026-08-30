pragma Singleton
import QtQuick 2.15

/**
 * 主题单例 —— macOS 原生融合风（方向 A）
 *
 * 配色基于 macOS 深色系统色阶 + 应用图标主色蓝 #5470FB：
 *  - 分层背景：window < sidebar < card < cardHover（由深到浅浮起）
 *  - 中性文字三级：text / secondary / tertiary
 *  - 阴影：shadowColor / shadowHoverColor（MultiEffect 投影）
 */
QtObject {
    // ===== 背景分层（由深到浅，模拟 macOS 窗口层级）=====
    readonly property color window: "#17171A"          // 主内容区底
    readonly property color sidebar: "#1D1D21"         // 侧边栏底（浅半档）
    readonly property color card: "#232327"            // 卡片
    readonly property color cardHover: "#2A2A2F"       // 卡片悬停
    readonly property color field: "#1F1F23"           // 输入框/内嵌区域

    // ===== 主色（取自应用图标）=====
    readonly property color primary: "#5470FB"
    readonly property color primaryHover: "#6B84FC"
    readonly property color primaryDim: "#3F54C4"      // 按下/暗态
    readonly property color primaryBg: "#1A2140"       // 主色极浅底（徽章等）

    // ===== 文字 =====
    readonly property color text: "#F5F5F7"            // macOS 深色主文字
    readonly property color textSecondary: "#98989F"
    readonly property color textTertiary: "#63636B"

    // ===== 语义色（macOS 系统色）=====
    readonly property color success: "#30D158"
    readonly property color warning: "#FF9F0A"
    readonly property color danger: "#FF453A"
    readonly property color dangerBg: "#3A1F1E"        // 危险色极浅底

    // ===== 分隔线 / 描边（半透明白，叠加在任何底色上都自然）=====
    readonly property color separator: "#10FFFFFF"     // 8% 白
    readonly property color cardBorder: "#14FFFFFF"    // 12% 白

    // ===== 圆角节奏 =====
    readonly property int radiusLarge: 12              // 卡片
    readonly property int radius: 8                    // 按钮/输入框
    readonly property int radiusSmall: 6               // 小元素

    // ===== 字级节奏 =====
    readonly property int fontTitle: 20                // 页面标题
    readonly property int fontHeadline: 15             // 卡片标题
    readonly property int fontNormal: 13               // 正文
    readonly property int fontSmall: 11                // 辅助
    readonly property int fontMini: 10                 // 徽章/标签

    // ===== 阴影（MultiEffect 参数）=====
    readonly property real shadowBlur: 0.5             // blurMax 比例
    readonly property color shadowColor: "#59000000"   // 静态投影
    readonly property int shadowYOff: 2
    readonly property color shadowHoverColor: "#73000000"  // 悬停加深
    readonly property int shadowHoverYOff: 5

    // ===== 动画时长 =====
    readonly property int animFast: 120
    readonly property int animNormal: 180
}
