import QtQuick
import QtQuick.Shapes
import MoyuLedger

// 预算环：底环 + 进度环（>=85% 转琥珀色表示接近超支）
Item {
    id: ring
    property real ratio: 0.0
    property bool near: false
    property string valueText: "—"
    property string caption: qsTr("已用")

    implicitWidth: Theme.ringDiameter
    implicitHeight: Theme.ringDiameter

    readonly property real stroke: Theme.ringStroke
    readonly property real centerX: width / 2
    readonly property real centerY: height / 2
    readonly property real radius: Math.min(width, height) / 2 - stroke / 2

    Shape {
        anchors.fill: parent
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            strokeColor: Theme.chartTrack
            strokeWidth: ring.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.centerX
                centerY: ring.centerY
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: -90
                sweepAngle: 359.9
            }
        }

        ShapePath {
            strokeColor: ring.near ? Theme.warnSolid : Theme.accent
            strokeWidth: ring.stroke
            fillColor: "transparent"
            capStyle: ShapePath.RoundCap
            PathAngleArc {
                centerX: ring.centerX
                centerY: ring.centerY
                radiusX: ring.radius
                radiusY: ring.radius
                startAngle: -90
                sweepAngle: 360 * Math.max(0, Math.min(1, ring.ratio))
            }
        }
    }

    Column {
        anchors.centerIn: parent
        spacing: 2

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: ring.valueText
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontKpi
            font.weight: Theme.weightSemibold
            font.features: ({ "tnum": 1 })
            color: Theme.textPrimary
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            text: ring.caption
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Theme.textTertiary
        }
    }
}