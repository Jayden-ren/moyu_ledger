import QtQuick
import MoyuLedger

// 线性导航图标（Canvas 自绘，不依赖图标字体，暗色主题自动跟随）
Item {
    id: icon
    property string name: "workbench"
    property color color: Theme.textSecondary
    property real strokeWidth: 1.4

    implicitWidth: 16
    implicitHeight: 16

    Canvas {
        id: canvas
        anchors.fill: parent
        contextType: "2d"

        onPaint: {
            const ctx = getContext("2d")
            const w = width
            const h = height
            const s = icon.strokeWidth
            ctx.reset()
            ctx.strokeStyle = icon.color
            ctx.fillStyle = icon.color
            ctx.lineWidth = s
            ctx.lineCap = "round"
            ctx.lineJoin = "round"

            const pad = 2.2
            const left = pad
            const top = pad
            const right = w - pad
            const bottom = h - pad
            const midX = (left + right) / 2
            const midY = (top + bottom) / 2

            if (icon.name === "workbench") {
                // 2×2 宫格
                const gap = 1.8
                const cw = (right - left - gap) / 2
                const ch = (bottom - top - gap) / 2
                ctx.strokeRect(left, top, cw, ch)
                ctx.strokeRect(left + cw + gap, top, cw, ch)
                ctx.strokeRect(left, top + ch + gap, cw, ch)
                ctx.strokeRect(left + cw + gap, top + ch + gap, cw, ch)
            } else if (icon.name === "overview") {
                // 柱状图
                const barW = (right - left) / 5
                ctx.beginPath()
                ctx.moveTo(left, bottom)
                ctx.lineTo(right, bottom)
                ctx.stroke()
                const heights = [0.45, 0.75, 1.0]
                for (let i = 0; i < 3; ++i) {
                    const x = left + barW * (0.6 + i * 1.5)
                    const bh = (bottom - top) * heights[i]
                    ctx.beginPath()
                    ctx.moveTo(x, bottom)
                    ctx.lineTo(x, bottom - bh)
                    ctx.stroke()
                }
            } else if (icon.name === "ledger") {
                // 三条横线
                for (let i = 0; i < 3; ++i) {
                    const y = top + (bottom - top) * i / 2
                    ctx.beginPath()
                    ctx.moveTo(left, y)
                    ctx.lineTo(right - (i === 1 ? 3 : 0), y)
                    ctx.stroke()
                }
            } else if (icon.name === "report") {
                // 折线图
                ctx.beginPath()
                ctx.moveTo(left, bottom)
                ctx.lineTo(right, bottom)
                ctx.stroke()
                ctx.beginPath()
                ctx.moveTo(left, bottom - 2)
                ctx.lineTo(left + (right - left) * 0.35, top + 4)
                ctx.lineTo(left + (right - left) * 0.62, top + 7)
                ctx.lineTo(right, top + 1)
                ctx.stroke()
            } else if (icon.name === "budget") {
                // 同心圆
                ctx.beginPath()
                ctx.arc(w / 2, h / 2, Math.min(w, h) / 2 - pad, 0, Math.PI * 2)
                ctx.stroke()
                ctx.beginPath()
                ctx.arc(w / 2, h / 2, Math.min(w, h) / 5, 0, Math.PI * 2)
                ctx.stroke()
            } else if (icon.name === "accounts") {
                // 卡片
                ctx.strokeRect(left, top + 1, right - left, bottom - top - 2)
                ctx.beginPath()
                ctx.moveTo(left, top + 5)
                ctx.lineTo(right, top + 5)
                ctx.stroke()
            } else if (icon.name === "settings") {
                // 齿轮：外圈 + 8 根辐条
                ctx.beginPath()
                ctx.arc(w / 2, h / 2, Math.min(w, h) / 3.4, 0, Math.PI * 2)
                ctx.stroke()
                const rIn = Math.min(w, h) / 3.4
                const rOut = Math.min(w, h) / 2 - pad + 0.6
                for (let i = 0; i < 8; ++i) {
                    const a = i * Math.PI / 4
                    ctx.beginPath()
                    ctx.moveTo(w / 2 + Math.cos(a) * rIn, h / 2 + Math.sin(a) * rIn)
                    ctx.lineTo(w / 2 + Math.cos(a) * rOut, h / 2 + Math.sin(a) * rOut)
                    ctx.stroke()
                }
            }
        }

        Connections {
            target: icon
            function onColorChanged() { canvas.requestPaint() }
            function onNameChanged() { canvas.requestPaint() }
        }
    }

    onColorChanged: canvas.requestPaint()
    onNameChanged: canvas.requestPaint()
}