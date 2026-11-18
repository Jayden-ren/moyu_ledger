import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 流水页：完整流水列表（不显示 KPI，专注浏览与筛选结果）
Item {
    id: page
    signal notifyRequested(string message)
    property alias currentRow: ledgerView.currentRow

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.toolBarH

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.pagePadding
                anchors.rightMargin: Theme.pagePadding
                spacing: Theme.spaceMd

                Text {
                    text: qsTr("流水")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH2
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Rectangle { width: 1; height: 18; color: Theme.borderBase }

                Text {
                    text: AppContext.currentMonth
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontBody2
                    color: Theme.textSecondary
                }

                Item { Layout.fillWidth: true }

                Text {
                    text: Ledger.hasActiveFilter
                          ? qsTr("筛选结果 %1 笔").arg(Ledger.transactionCount)
                          : qsTr("共 %1 笔").arg(Ledger.transactionCount)
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textSecondary
                }

                FlatButton {
                    visible: Ledger.hasActiveFilter
                    text: qsTr("清除筛选")
                    kind: "ghost"
                    onClicked: {
                        Ledger.clearFilter()
                        page.notifyRequested(qsTr("已清除筛选"))
                    }
                }
            }
        }

        TableHeader {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.colHeaderH
        }

        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true

            TransactionListView {
                id: ledgerView
                anchors.fill: parent
                model: Ledger.transactionModel
                visible: Ledger.transactionCount > 0
            }

            Column {
                anchors.centerIn: parent
                spacing: Theme.spaceSm
                visible: Ledger.transactionCount === 0

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Ledger.hasActiveFilter ? qsTr("没有符合条件的流水") : qsTr("这个月还没有记账")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontTitle
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Text {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: Ledger.hasActiveFilter ? qsTr("试试换个关键字，或清除筛选") : qsTr("按 Ctrl+N 记一笔")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textSecondary
                }
            }
        }

        StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.statusBarH
        }
    }
}