import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 底部状态条 34px：可见条数 + 统一键位提示
Rectangle {
    id: statusBar
    height: Theme.statusBarH
    color: Theme.bgSubtle

    readonly property var hints: [
        { key: "↑↓", label: qsTr("选择") },
        { key: "Enter", label: qsTr("详情") },
        { key: "Ctrl N", label: qsTr("记一笔") },
        { key: "Ctrl Z", label: qsTr("撤销") },
        { key: "Ctrl K", label: qsTr("命令") }
    ]

    Rectangle {
        width: parent.width
        height: 1
        anchors.top: parent.top
        color: Theme.borderBase
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.spaceLg
        anchors.rightMargin: Theme.spaceLg
        spacing: Theme.spaceMd

        Text {
            text: Ledger.filterActive
                  ? (qsTr("已筛选 ") + Ledger.filteredCount + qsTr(" 笔 / 共 ")
                     + Ledger.countText + qsTr(" 笔") + " · " + Ledger.filterSummary)
                  : (qsTr("已显示 ") + Ledger.countText + qsTr(" 笔"))
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontMicro3
            color: Ledger.filterActive ? Theme.accent : Theme.textTertiary
        }

        Item { Layout.fillWidth: true }

        Row {
            spacing: Theme.spaceMd

            Repeater {
                model: statusBar.hints

                delegate: Row {
                    required property var modelData
                    spacing: Theme.spaceXs

                    KeyCap { text: modelData.key }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }
                }
            }
        }
    }
}