import QtQuick
import MoyuLedger

// 分组柱状图：每月一对柱子（支出 / 收入），纯 Rectangle 绘制
Item {
    id: chart
    property var points: []          // [{ label, expenseCents, incomeCents }]
    property double barMaxHeight: 140

    readonly property real maxValue: {
        let max = 1
        for (let i = 0; i < points.length; ++i) {
            max = Math.max(max, points[i].expenseCents || 0, points[i].incomeCents || 0)
        }
        return max
    }

    Row {
        anchors.fill: parent
        anchors.bottomMargin: 18
        spacing: 0

        Repeater {
            model: chart.points

            delegate: Item {
                required property int index
                required property var modelData

                width: chart.width / Math.max(1, chart.points.length)
                height: chart.height

                Row {
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 3

                    Rectangle {
                        width: Math.max(6, Math.min(14, parent.parent.width / 5))
                        height: Math.max(2, chart.barMaxHeight * (modelData.expenseCents || 0) / chart.maxValue)
                        anchors.bottom: parent.bottom
                        radius: 2
                        color: Theme.accent
                    }

                    Rectangle {
                        width: Math.max(6, Math.min(14, parent.parent.width / 5))
                        height: Math.max(2, chart.barMaxHeight * (modelData.incomeCents || 0) / chart.maxValue)
                        anchors.bottom: parent.bottom
                        radius: 2
                        color: Theme.income
                        opacity: 0.75
                    }
                }

                Text {
                    anchors.bottom: parent.bottom
                    anchors.bottomMargin: -18
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: modelData.label
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontMicro2
                    color: Theme.textTertiary
                }
            }
        }
    }
}