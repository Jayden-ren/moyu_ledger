import QtQuick
import QtQuick.Layouts
import MoyuLedger

// KPI 条 68px：五格等分，全部来自 Ledger（本月汇总）
Rectangle {
    id: strip
    height: Theme.kpiStripH
    color: Theme.bgSurface

    property var cells: [
        { label: qsTr("本月收入"), value: Ledger.incomeText, sub: qsTr("本月收入合计") },
        { label: qsTr("本月支出"), value: Ledger.expenseText, sub: qsTr("日均 ") + Ledger.dayAverageText },
        { label: qsTr("本月结余"), value: Ledger.balanceText, sub: qsTr("收入 − 支出") },
        {
            label: qsTr("预算已用"),
            value: Ledger.budgetUsedText,
            sub: qsTr("额度 ") + Ledger.budgetLimitText,
            progress: Ledger.budgetRatio,
            near: Ledger.budgetNearLimit
        },
        {
            label: qsTr("预算剩余"),
            value: Ledger.hasBudget ? Ledger.budgetRemainText : "—",
            sub: !Ledger.hasBudget ? qsTr("尚未设置预算")
                                   : (Ledger.budgetNearLimit ? qsTr("接近超支") : qsTr("额度充足"))
        }
    ]

    RowLayout {
        anchors.fill: parent
        spacing: 0

        Repeater {
            model: strip.cells

            delegate: Rectangle {
                id: cell
                required property int index
                required property var modelData

                Layout.fillWidth: true
                Layout.fillHeight: true
                color: "transparent"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.leftMargin: Theme.spaceLg + 2
                    anchors.rightMargin: Theme.spaceLg
                    anchors.topMargin: Theme.spaceMd
                    anchors.bottomMargin: Theme.spaceMd
                    spacing: 1

                    Text {
                        text: cell.modelData.label
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontMicro3
                        color: Theme.textTertiary
                    }

                    Text {
                        text: cell.modelData.value
                        font.family: Theme.fontFamily
                        font.pixelSize: Theme.fontTitle
                        font.weight: Theme.weightSemibold
                        color: Theme.textPrimary
                        font.features: ({ "tnum": 1 })
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Theme.spaceS

                        Rectangle {
                            visible: cell.modelData.progress !== undefined
                            Layout.fillWidth: true
                            Layout.preferredHeight: 4
                            Layout.maximumWidth: 72
                            radius: 2
                            color: Theme.chartTrack

                            Rectangle {
                                width: parent.width * (cell.modelData.progress || 0)
                                height: parent.height
                                radius: 2
                                color: cell.modelData.near ? Theme.warnSolid : Theme.accent
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            text: cell.modelData.sub
                            elide: Text.ElideRight
                            font.family: Theme.fontFamily
                            font.pixelSize: Theme.fontMicro3
                            color: Theme.textTertiary
                        }
                    }
                }

                Rectangle {
                    visible: cell.index < 4
                    width: 1
                    height: parent.height - Theme.spaceXxl
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.borderDivider
                }
            }
        }
    }

    Rectangle {
        height: 1
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: Theme.borderBase
    }
}