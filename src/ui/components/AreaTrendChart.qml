import QtQuick
import MoyuLedger

// 近 N 月支出趋势：面积填充 + 折线 + 数据点 + 网格 + 月份标签
// 用 Canvas 绘制（Shape 的 PathPolyline.path 绑定在数据变化时会丢中间点，导致面积变成一条大斜边）
Item {
    id: chart
    property var points: []          // [{ label, expenseCents }]
    property color lineColor: Theme.chartLine
    property color fillTopColor: Theme.chartFillTop
    property color gridColor: Theme.borderHairline
    property color dotFill: Theme.bgSurface
    property color labelColor: Theme.textTertiary

    readonly property real paddingLeft: 10
    readonly property real paddingRight: 10
    readonly property real paddingTop: 18
    readonly property real paddingBottom: 22
    readonly property real plotWidth: Math.max(1, width - paddingLeft - paddingRight)
    readonly property real plotHeight: Math.max(1, height - paddingTop - paddingBottom)
    readonly property real baselineY: paddingTop + plotHeight
    readonly property real maxValue: {
        let max = 1
        for (let i = 0; i < points.length; ++i)
            max = Math.max(max, points[i].expenseCents || 0)
        return max * 1.15
    }

    function xAt(index) {
        if (points.length <= 1)
            return paddingLeft + plotWidth / 2
        return paddingLeft + plotWidth * index / (points.length - 1)
    }

    function yAt(value) {
        return paddingTop + plotHeight * (1 - (value || 0) / maxValue)
    }

    function repaint() { canvas.requestPaint() }

    onWidthChanged: repaint()
    onHeightChanged: repaint()
    onPointsChanged: repaint()
    onLineColorChanged: repaint()
    onFillTopColorChanged: repaint()
    Component.onCompleted: repaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Immediate
        contextType: "2d"

        onPaint: {
            const ctx = getContext("2d")
            const w = width
            const h = height
            if (w <= 2 || h <= 2)
                return

            ctx.reset()
            ctx.clearRect(0, 0, w, h)

            const n = chart.points.length
            if (n === 0)
                return

            const dpr = (typeof window !== "undefined" && window.devicePixelRatio) ? window.devicePixelRatio : 1
            if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
                canvas.width = Math.round(w * dpr)
                canvas.height = Math.round(h * dpr)
            }
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
            ctx.clearRect(0, 0, w, h)

            // ---- 网格线 ----
            ctx.strokeStyle = chart.gridColor
            ctx.lineWidth = 1
            ctx.setLineDash([3, 4])
            for (let g = 0; g <= 3; ++g) {
                const y = chart.paddingTop + chart.plotHeight * g / 3
                ctx.beginPath()
                ctx.moveTo(chart.paddingLeft, y)
                ctx.lineTo(w - chart.paddingRight, y)
                ctx.stroke()
            }
            ctx.setLineDash([])

            // ---- 面积填充 ----
            const gradient = ctx.createLinearGradient(0, chart.paddingTop, 0, chart.baselineY)
            gradient.addColorStop(0, chart.fillTopColor)
            gradient.addColorStop(1, "transparent")

            ctx.beginPath()
            ctx.moveTo(chart.xAt(0), chart.baselineY)
            for (let i = 0; i < n; ++i)
                ctx.lineTo(chart.xAt(i), chart.yAt(chart.points[i].expenseCents))
            ctx.lineTo(chart.xAt(n - 1), chart.baselineY)
            ctx.closePath()
            ctx.fillStyle = gradient
            ctx.fill()

            // ---- 折线 ----
            ctx.beginPath()
            for (let i = 0; i < n; ++i) {
                const x = chart.xAt(i)
                const y = chart.yAt(chart.points[i].expenseCents)
                if (i === 0)
                    ctx.moveTo(x, y)
                else
                    ctx.lineTo(x, y)
            }
            ctx.strokeStyle = chart.lineColor
            ctx.lineWidth = 2
            ctx.lineJoin = "round"
            ctx.lineCap = "round"
            ctx.stroke()

            // ---- 数据点 ----
            for (let i = 0; i < n; ++i) {
                const x = chart.xAt(i)
                const y = chart.yAt(chart.points[i].expenseCents)
                ctx.beginPath()
                ctx.arc(x, y, 3.2, 0, Math.PI * 2)
                ctx.fillStyle = chart.dotFill
                ctx.fill()
                ctx.strokeStyle = chart.lineColor
                ctx.lineWidth = 2
                ctx.stroke()
            }

            // ---- 月份标签 ----
            ctx.fillStyle = chart.labelColor
            ctx.font = "11px sans-serif"
            ctx.textAlign = "center"
            ctx.textBaseline = "top"
            for (let i = 0; i < n; ++i) {
                const label = chart.points[i].label || ""
                if (label.length === 0)
                    continue
                ctx.fillText(label, chart.xAt(i), chart.baselineY + 6)
            }
        }
    }
}