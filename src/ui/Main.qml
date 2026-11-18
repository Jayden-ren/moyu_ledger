import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import MoyuLedger

ApplicationWindow {
    id: appWindow
    width: Theme.windowW
    height: Theme.windowH
    minimumWidth: Theme.windowMinW
    minimumHeight: Theme.windowMinH
    visible: true
    title: qsTr("摸鱼记账")
    color: Theme.bgCanvas
    flags: Qt.Window | Qt.FramelessWindowHint

    function notify(message) {
        toast.text = message
        toast.opacity = 1
        toastTimer.restart()
    }

    // file:/// URL → 本地路径（Windows 反斜杠）
    function localPath(url) {
        let text = String(url)
        if (text.indexOf("file:///") === 0)
            text = text.substring(8)
        else if (text.indexOf("file://") === 0)
            text = text.substring(7)
        return decodeURIComponent(text)
    }

    // 无系统边框时的 1px 描边（不拦截鼠标）
    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Theme.borderBase
        z: 1000
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        AppTitleBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.titleBarH
            hostWindow: appWindow
            z: 900   // 账本切换浮层要盖住下方内容区
        }

        AppToolBar {
            id: toolBar
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.toolBarH
            onAddRequested: openRecordDialog()
            onCommandRequested: commandPalette.open()
            onExportRequested: exportBackupDialog.open()
            onImportRequested: importBackupDialog.open()
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            NavRail {
                id: navRail
                Layout.preferredWidth: Theme.navW
                Layout.fillHeight: true
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                currentIndex: navRail.currentIndex

                WorkbenchPage {
                    id: workbench
                    onAddRequested: openRecordDialog()
                    onImportRequested: importDialog.open()
                    onOpenBudgetRequested: navRail.currentIndex = 4
                    onTransactionActivated: function(index) {
            const detail = Ledger.transactionModel.rowData(index)
            if (detail && detail.transactionId !== undefined)
                recordDialog.openForEdit(detail)
        }        onAccountRequested: accountDialog.open()
                }

                OverviewPage {
                    onOpenBudgetRequested: navRail.currentIndex = 4
                }

                LedgerPage {
                    onNotifyRequested: function(message) { appWindow.notify(message) }
                }
                ReportsPage {
                    id: reportsPage
                    onNotifyRequested: function(message) { appWindow.notify(message) }
                }
                BudgetPage {
                    id: budgetPage
                    onNotifyRequested: function(message) { appWindow.notify(message) }
                }
                AccountsPage {
                    id: accountsPage
                    onAddRequested: accountDialog.open()
                    onEditRequested: function(detail) { accountDialog.openForAccount(detail) }
                    onArchiveRequested: function(id, archived) {
                        const result = Ledger.archiveAccount(id, archived)
                        appWindow.notify(result.message)
                    }
                }
                SettingsPage {
                    onNotifyRequested: function(message) { appWindow.notify(message) }
                    onAddRecurringRequested: recurringDialog.open()
                    onManageCategoriesRequested: categoryDialog.open()
                    onEditRecurringRequested: function(rule) { recurringDialog.openForRule(rule) }
                }
            }

            DetailPane {
                visible: navRail.currentIndex === 0
                Layout.preferredWidth: visible ? Theme.detailW : 0
                Layout.fillHeight: true
                detail: {
                    // 依赖隐私开关，切换「隐藏金额」时详情里的金额也一起刷新
                    const privacy = Ledger.privacyMode
                    return (visible && workbench.currentRow >= 0 && privacy !== undefined)
                           ? Ledger.transactionModel.rowData(workbench.currentRow) : ({})
                }
                onDeleteRequested: function(id) {
                    const result = Ledger.removeTransaction(id)
                    workbench.currentRow = -1
                    appWindow.notify(result.message)
                }
                onEditRequested: function(detail) { recordDialog.openForEdit(detail) }
                onDuplicateRequested: appWindow.notify(qsTr("「记一笔类似」将在下一轮实现"))
            }
        }
    }

    // 主题：Theme.dark 跟随 AppContext（暗色 token 早已就绪）
    Binding {
        target: Theme
        property: "dark"
        value: AppContext.darkTheme
    }

    // 统一入口：没有账户时先引导创建账户
    function openRecordDialog() {
        if (Ledger.accountModel.count === 0) {
            appWindow.notify(qsTr("请先创建一个账户，再开始记账"))
            accountDialog.open()
            return
        }
        recordDialog.open()
    }

    Component.onCompleted: {
        Ledger.setMonth(AppContext.year, AppContext.month)
        // 恢复上次的主题选择（settings.theme.dark）
        AppContext.darkTheme = Ledger.themeDark()
        // 补生成本月到期的定期账单（幂等，重复启动不会重复记账）
        const generated = Ledger.generateRecurring()
        if (generated.ok)
            appWindow.notify(generated.message)
    }

    Connections {
        target: AppContext
        function onDarkThemeChanged() { Ledger.saveThemeDark(AppContext.darkTheme) }
    }

    Connections {
        target: AppContext
        function onMonthChanged() {
            Ledger.setMonth(AppContext.year, AppContext.month)
        }
    }

    // ---- 全局快捷键（与状态条、命令面板共用同一套键位）----
    Shortcut { sequence: "Ctrl+N"; onActivated: openRecordDialog() }
    Shortcut { sequence: "Ctrl+K"; onActivated: commandPalette.open() }
    Shortcut { sequence: "Ctrl+F"; onActivated: toolBar.searchField.forceActiveFocus() }
    Shortcut { sequence: "Ctrl+1"; onActivated: navRail.currentIndex = 0 }
    Shortcut { sequence: "Ctrl+2"; onActivated: navRail.currentIndex = 1 }
    Shortcut { sequence: "Ctrl+3"; onActivated: navRail.currentIndex = 2 }
    Shortcut { sequence: "Ctrl+4"; onActivated: navRail.currentIndex = 3 }
    Shortcut { sequence: "Ctrl+5"; onActivated: navRail.currentIndex = 4 }
    Shortcut { sequence: "Ctrl+6"; onActivated: navRail.currentIndex = 5 }
    Shortcut { sequence: "Ctrl+7"; onActivated: navRail.currentIndex = 6 }
    Shortcut {
        sequence: "Ctrl+E"
        onActivated: appWindow.notify(Ledger.exportMonthCsv("").message)
    }
    Shortcut { sequence: "Ctrl+T"; onActivated: AppContext.goToCurrentMonth() }
    Shortcut { sequence: "Ctrl+Shift+L"; onActivated: AppContext.toggleDarkTheme() }
    Shortcut {
        sequence: "Ctrl+Z"
        onActivated: {
            const result = Ledger.undo()
            workbench.currentRow = -1
            appWindow.notify(result.message)
        }
    }
    Shortcut {
        sequence: "Ctrl+Shift+Z"
        onActivated: {
            const result = Ledger.redo()
            workbench.currentRow = -1
            appWindow.notify(result.message)
        }
    }

    Rectangle {
        id: toast
        property string text: ""
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Theme.statusBarH + Theme.spaceXxxl
        width: toastLabel.implicitWidth + Theme.spaceXxxl
        height: 32
        radius: Theme.radiusBtn
        color: Theme.textPrimary
        opacity: 0
        visible: opacity > 0
        z: 999

        Behavior on opacity { NumberAnimation { duration: Theme.motionBase } }

        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: toast.text
            font.family: Theme.fontFamily
            font.pixelSize: Theme.fontSmall2
            color: Theme.textInverse
        }
    }

    Timer {
        id: toastTimer
        interval: 1800
        onTriggered: toast.opacity = 0
    }

    AccountDialog {
        id: accountDialog
        onSaved: function(message) { appWindow.notify(message) }
    }

    CategoryDialog {
        id: categoryDialog
        onNotifyRequested: function(message) { appWindow.notify(message) }
    }

    RecurringDialog {
        id: recurringDialog
        onNotifyRequested: function(message) { appWindow.notify(message) }
    }

    ImportDialog {
        id: importDialog
        onNotifyRequested: function(message) { appWindow.notify(message) }
    }

    CommandPalette {
        id: commandPalette
        onNotifyRequested: function(message) { appWindow.notify(message) }
        onNavigateRequested: function(index) { navRail.currentIndex = index }
        onRecordDialogHook: openRecordDialog()
        onImportDialogHook: importDialog.open()
        onCategoryDialogHook: categoryDialog.open()
    }



    // ===== 全量备份：导出 / 导入（账户 + 预算 + 全部流水）=====
    FileDialog {
        id: exportBackupDialog
        title: qsTr("导出全量备份（账户 / 预算 / 全部流水）")
        fileMode: FileDialog.SaveFile
        defaultSuffix: "json"
        nameFilters: [qsTr("JSON 备份 (*.json)")]
        onAccepted: {
            const result = Ledger.exportBackup(appWindow.localPath(exportBackupDialog.selectedFile))
            appWindow.notify(result.message)
        }
    }

    FileDialog {
        id: importBackupDialog
        title: qsTr("选择要导入的全量备份")
        fileMode: FileDialog.OpenFile
        nameFilters: [qsTr("JSON 备份 (*.json)")]
        onAccepted: importConfirm.open(appWindow.localPath(importBackupDialog.selectedFile))
    }

    Item {
        id: importConfirm
        anchors.fill: parent
        visible: false
        z: 960

        property string filePath: ""
        property string fileName: ""

        function open(path) {
            filePath = path
            const parts = String(path).split(/[\\/]/)
            fileName = parts.length > 0 ? parts[parts.length - 1] : path
            visible = true
        }

        function close() { visible = false }

        function doImport() {
            const result = Ledger.importBackup(importConfirm.filePath)
            appWindow.notify(result.message)
            if (result.ok) {
                workbench.currentRow = -1
                navRail.currentIndex = 0
            }
            close()
        }

        Rectangle {
            anchors.fill: parent
            color: Theme.scrim
            MouseArea { anchors.fill: parent; onClicked: importConfirm.close() }
        }

        Rectangle {
            width: 470
            height: 228
            anchors.centerIn: parent
            radius: Theme.radiusPanel
            color: Theme.bgSurface
            border.width: 1
            border.color: Theme.borderBase

            MouseArea { anchors.fill: parent }

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.spaceXl
                spacing: Theme.spaceS

                Text {
                    text: qsTr("导入全量备份")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontH1
                    font.weight: Theme.weightSemibold
                    color: Theme.textPrimary
                }

                Text {
                    Layout.fillWidth: true
                    text: importConfirm.fileName
                    elide: Text.ElideMiddle
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.accent
                }

                Text {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    wrapMode: Text.WordWrap
                    text: qsTr("将用备份文件里的内容替换当前所有账户、分类、预算与流水。\n导入前会自动备份当前数据库，可在 %LOCALAPPDATA%\\MoyuLedger\\backups 找回。")
                    font.family: Theme.fontFamily
                    font.pixelSize: Theme.fontSmall2
                    color: Theme.textSecondary
                }

                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Row {
                        spacing: Theme.spaceMd
                        FlatButton { text: qsTr("取消"); kind: "ghost"; onClicked: importConfirm.close() }
                        FlatButton { text: qsTr("确认导入"); kind: "primary"; onClicked: importConfirm.doImport() }
                    }
                }
            }
        }
    }
    DailyRecordDialog {
        id: dailyRecordDialog
        onSaved: function(message) { appWindow.notify(message) }
    }

    RecordDialog {
        id: recordDialog
        onAccountCreationRequested: {
            appWindow.notify(qsTr("请先创建一个账户，再开始记账"))
            accountDialog.open()
        }
        onSaved: function(message) { appWindow.notify(message) }
    }

}
