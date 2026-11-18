import QtQuick
import QtQuick.Layouts
import MoyuLedger

// 工作台页：KPI 条 + 列头 + 流水列表（或空状态）+ 状态条
Item {
    id: page
    signal addRequested()
    signal importRequested()
    signal openBudgetRequested()
    signal accountRequested()
    signal transactionActivated(int index)
    property alias currentRow: ledgerView.currentRow

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        BudgetAlert {
            Layout.fillWidth: true
            Layout.preferredHeight: visible ? 34 : 0
            level: Ledger.budgetAlertLevel
            text: Ledger.budgetAlertText
            onOpenRequested: page.openBudgetRequested()
        }

        KpiStrip {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.kpiStripH
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
                onRowActivated: function(index) { page.transactionActivated(index) }
            }

            EmptyLedgerState {
                anchors.fill: parent
                visible: Ledger.transactionCount === 0
                needsAccount: Ledger.accountModel.count === 0
                onAddRequested: page.addRequested()
                onImportRequested: page.importRequested()
                onAccountRequested: page.accountRequested()
            }
        }

        StatusBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.statusBarH
        }
    }
}