import QtQuick 2.15

/**
 * 统一的矢量线条图标组件（16×16 逻辑网格，Canvas 绘制）。
 * 比 emoji 更干净：单色、线宽一致、与文字颜色联动。
 *
 * 用法: AppIcon { name: "folder"; iconColor: Theme.textSecondary }
 * 支持: folder / trash / doc / restart / close / terminal
 */
Canvas {
    id: root

    property string name: ""
    property color iconColor: "#b0b0b0"
    property real strokeWidth: 1.5

    width: 16
    height: 16

    // Retina 高清渲染：按设备像素比放大画布
    readonly property real dpr: Screen.devicePixelRatio
    canvasSize: Qt.size(width * dpr, height * dpr)

    onNameChanged: requestPaint()
    onIconColorChanged: requestPaint()

    onPaint: {
        const ctx = getContext("2d")
        ctx.reset()
        ctx.scale(dpr, dpr)
        ctx.strokeStyle = iconColor
        ctx.lineWidth = strokeWidth
        ctx.lineCap = "round"
        ctx.lineJoin = "round"

        switch (root.name) {
        case "folder":      // 打开目录
            ctx.beginPath()
            ctx.moveTo(2.5, 12.5)
            ctx.lineTo(2.5, 4.5)
            ctx.lineTo(6, 4.5)
            ctx.lineTo(7.5, 6.5)
            ctx.lineTo(13.5, 6.5)
            ctx.lineTo(13.5, 12.5)
            ctx.closePath()
            ctx.stroke()
            break

        case "trash":       // 卸载
            // 桶盖
            ctx.beginPath()
            ctx.moveTo(3, 4.5)
            ctx.lineTo(13, 4.5)
            ctx.stroke()
            // 把手
            ctx.beginPath()
            ctx.moveTo(6.5, 4.5)
            ctx.lineTo(6.5, 3)
            ctx.lineTo(9.5, 3)
            ctx.lineTo(9.5, 4.5)
            ctx.stroke()
            // 桶身
            ctx.beginPath()
            ctx.moveTo(4, 4.5)
            ctx.lineTo(4.6, 13)
            ctx.lineTo(11.4, 13)
            ctx.lineTo(12, 4.5)
            ctx.stroke()
            // 桶身竖纹
            ctx.beginPath()
            ctx.moveTo(6.8, 7)
            ctx.lineTo(7, 11)
            ctx.moveTo(9.2, 7)
            ctx.lineTo(9, 11)
            ctx.stroke()
            break

        case "doc":         // 查看输出（文档 + 文本行）
            ctx.beginPath()
            ctx.moveTo(4, 2.5)
            ctx.lineTo(12, 2.5)
            ctx.lineTo(12, 13.5)
            ctx.lineTo(4, 13.5)
            ctx.closePath()
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(6, 6)
            ctx.lineTo(10, 6)
            ctx.moveTo(6, 8.5)
            ctx.lineTo(10, 8.5)
            ctx.moveTo(6, 11)
            ctx.lineTo(9, 11)
            ctx.stroke()
            break

        case "restart": {   // 重启（圆弧 + 箭头）
            const cx = 8, cy = 8.2, r = 5
            const a0 = Math.PI * 0.35   // 起点（左下）
            const a1 = Math.PI * 2.1    // 终点（顶部偏右，留缺口）
            ctx.beginPath()
            ctx.arc(cx, cy, r, a0, a1)
            ctx.stroke()
            // 箭头：沿终点切线方向
            const ex = cx + r * Math.cos(a1)
            const ey = cy + r * Math.sin(a1)
            const ta = a1 + Math.PI / 2
            const s = 2.8
            ctx.beginPath()
            ctx.moveTo(ex - s * Math.cos(ta - 0.55), ey - s * Math.sin(ta - 0.55))
            ctx.lineTo(ex, ey)
            ctx.lineTo(ex - s * Math.cos(ta + 0.55), ey - s * Math.sin(ta + 0.55))
            ctx.stroke()
            break
        }

        case "close":       // 关闭
            ctx.beginPath()
            ctx.moveTo(4.5, 4.5)
            ctx.lineTo(11.5, 11.5)
            ctx.moveTo(11.5, 4.5)
            ctx.lineTo(4.5, 11.5)
            ctx.stroke()
            break

        case "terminal":    // 附加到终端（> 提示符 + _ 光标）
            ctx.beginPath()
            ctx.moveTo(4, 5)
            ctx.lineTo(7.5, 8)
            ctx.lineTo(4, 11)
            ctx.stroke()
            ctx.beginPath()
            ctx.moveTo(9, 11.5)
            ctx.lineTo(13, 11.5)
            ctx.stroke()
            break

        case "grid": {      // 插件管理（四宫格圆角方块）
            const g = 1.6                    // 间距
            const cell = (16 - 5 - g) / 2    // 单元格边长 ≈ 4.7
            const cr = 1.3                   // 单元格圆角
            const ox = 2.5, oy = 2.5
            for (let i = 0; i < 4; ++i) {
                const cx0 = ox + (i % 2) * (cell + g)
                const cy0 = oy + Math.floor(i / 2) * (cell + g)
                ctx.beginPath()
                ctx.moveTo(cx0 + cr, cy0)
                ctx.lineTo(cx0 + cell - cr, cy0)
                ctx.arcTo(cx0 + cell, cy0, cx0 + cell, cy0 + cr, cr)
                ctx.lineTo(cx0 + cell, cy0 + cell - cr)
                ctx.arcTo(cx0 + cell, cy0 + cell, cx0 + cell - cr, cy0 + cell, cr)
                ctx.lineTo(cx0 + cr, cy0 + cell)
                ctx.arcTo(cx0, cy0 + cell, cx0, cy0 + cell - cr, cr)
                ctx.lineTo(cx0, cy0 + cr)
                ctx.arcTo(cx0, cy0, cx0 + cr, cy0, cr)
                ctx.closePath()
                ctx.stroke()
            }
            break
        }

        case "gear": {      // 设置（齿轮）
            const gcx = 8, gcy = 8
            // 齿：8 根辐条
            for (let i = 0; i < 8; ++i) {
                const a = i * Math.PI / 4
                ctx.beginPath()
                ctx.moveTo(gcx + 4.4 * Math.cos(a), gcy + 4.4 * Math.sin(a))
                ctx.lineTo(gcx + 5.9 * Math.cos(a), gcy + 5.9 * Math.sin(a))
                ctx.stroke()
            }
            // 外圈
            ctx.beginPath()
            ctx.arc(gcx, gcy, 4.4, 0, Math.PI * 2)
            ctx.stroke()
            // 中心孔
            ctx.beginPath()
            ctx.arc(gcx, gcy, 1.7, 0, Math.PI * 2)
            ctx.stroke()
            break
        }
        }
    }
}
