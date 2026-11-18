import QtQuick
import MoyuLedger

// 分类占比堆叠条：按 share 宽度分配，段间 1px 白缝
Item {
    id: bar
    property var items: []       // [{ name, color, share, totalText }]
    property real barHeight: Theme.stackedBarH

    height: barHeight

    Row {
        anchors.fill: parent
        spacing: 1

        Repeater {
            model: bar.items

            delegate: Rectangle {
                required property var modelData
                height: bar.barHeight
                width: Math.max(3, Math.round(modelData.share * bar.width))
                color: modelData.color
                radius: 2
            }
        }
    }
}