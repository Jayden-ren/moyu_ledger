# 可执行目标 + QML 模块（模块根目录 = src/ui，资源路径 qrc:/qt/qml/MoyuLedger/...）
qt_add_executable(MoyuLedger
    WIN32 MACOSX_BUNDLE
    ${CMAKE_SOURCE_DIR}/main.cpp
)

qt_policy(SET QTP0004 NEW)   # 子目录自动生成 qmldir（内容为 prefer :）
qt_policy(SET QTP0001 NEW)   # 资源前缀采用 :/qt/qml/

set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(MoyuLedger
    URI MoyuLedger
    VERSION 1.0
    QML_FILES
        Main.qml
        Theme.qml
        components/AppTitleBar.qml
        components/AppToolBar.qml
        components/NavRail.qml
        components/KpiStrip.qml
        components/TableHeader.qml
        components/EmptyLedgerState.qml
        components/StatusBar.qml
        components/DetailPane.qml
        components/TransactionListView.qml
        components/FlatButton.qml
        components/WindowButton.qml
        components/KeyCap.qml
        components/NavIcon.qml
        components/LedgerSwitcher.qml
        components/TimePicker.qml
        components/DatePicker.qml
        components/BudgetRing.qml
        components/AreaTrendChart.qml
        components/StackedBar.qml
        components/FilterPanel.qml
        components/GroupedBars.qml
        components/DonutChart.qml
        components/BudgetAlert.qml
        pages/WorkbenchPage.qml
        pages/OverviewPage.qml
        pages/AccountsPage.qml
        pages/LedgerPage.qml
        pages/SettingsPage.qml
        pages/ReportsPage.qml
        pages/BudgetPage.qml
        dialogs/RecordDialog.qml
        dialogs/DailyRecordDialog.qml
        dialogs/AccountDialog.qml
        dialogs/CommandPalette.qml
        dialogs/ImportDialog.qml
        dialogs/RecurringDialog.qml
        dialogs/CategoryDialog.qml
    SOURCES
        AppContext.h
        AppContext.cpp
)

# QML_ELEMENT 生成的注册代码用 #include <AppContext.h>
target_include_directories(MoyuLedger PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})