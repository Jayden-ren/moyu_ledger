import QtQuick
import MoyuLedger

// 环形图：按占比画扇区（Canvas），中心显示合计或自定义文案
Item {
    id: donut
    property var items: []              // [{ name, color, share, totalText }]
    property string centerTitle: ""
    property string centerCaption: ""
    property int ringWidth: 14
    property color trackColor: Theme.chartTrack
    property color titleColor: Theme.textPrimary
    property color captionColor: Theme.textTertiary

    function repaint() { canvas.requestPaint() }

    onWidthChanged: repaint()
    onHeightChanged: repaint()
    onItemsChanged: repaint()
    onRingWidthChanged: repaint()
    Component.onCompleted: repaint()

    Canvas {
        id: canvas
        anchors.fill: parent
        renderStrategy: Canvas.Immediate

        onPaint: {
            const ctx = getContext("2d")
            const w = width
            const h = height
            if (w <= 4 || h <= 4)
                return

            const dpr = (typeof window !== "undefined" && window.devicePixelRatio) ? window.devicePixelRatio : 1
            if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
                canvas.width = Math.round(w * dpr)
                canvas.height = Math.round(h * dpr)
            }
            ctx.setTransform(dpr, 0, 0, dpr, 0, 0)
            ctx.clearRect(0, 0, w, h)

            const cx = w / 2
            const cy = h / 2
            const outer = Math.min(w, h) / 2 - 2
            const inner = Math.max(4, outer - donut.ringWidth)
            const mid = (outer + inner) / 2

            // 底环
            ctx.strokeStyle = donut.trackColor
            ctx.lineWidth = donut.ringWidth
            ctx.beginPath()
            ctx.arc(cx, cy, mid, 0, Math.PI * 2)
            ctx.stroke()

            // 扇区
            let start = -Math.PI / 2
            for (let i = 0; i < donut.items.length; ++i) {
                const share = Math.max(0, Math.min(1, donut.items[i].share || 0))
                if (share <= 0)
                    continue
                const angle = share * Math.PI * 2
                ctx.strokeStyle = donut.items[i].color || "#999999"
                ctx.lineWidth = donut.ringWidth
                ctx.lineCap = "butt"
                ctx.beginPath()
                ctx.arc(cx, cy, mid, start, start + angle)
                ctx.stroke()
                start += angle
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 0

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: donut.centerTitle
            font.family: Theme.fontFamily
            font.pixelSize: donut.width > 180 ? Theme.fontH2 : Theme.fontBody
            font.weight: Theme.weightSemibold
            color: donut.titleColor
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: donut.centerCaption
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: donut.captionColor
        }
    }
}