#pragma once

#include "core/data/AccountDailyRecordRepository.h"
#include "core/data/AccountRepository.h"
#include "core/data/BudgetRepository.h"
#include "core/data/CategoryRepository.h"
#include "core/data/LedgerRepository.h"
#include "core/data/RecurringRepository.h"
#include "core/data/SettingsRepository.h"
#include "core/domain/Privacy.h"

#include <QJsonObject>
#include "core/data/TagRepository.h"
#include "core/data/TransactionRepository.h"
#include "core/model/AccountListModel.h"
#include "core/model/TransactionListModel.h"

#include <QObject>
#include <QUndoStack>
#include <QSqlDatabase>
#include <QString>
#include <QVariantList>

namespace keep {

// QML 侧的数据门面：KPI 文本、流水列表、账户列表、分类占比、趋势。
// 由 main.cpp 创建后通过 qmlRegisterSingletonInstance 暴露为 QML 单例 `Ledger`。
class LedgerViewModel : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString incomeText READ incomeText NOTIFY summaryChanged)
    Q_PROPERTY(QString expenseText READ expenseText NOTIFY summaryChanged)
    Q_PROPERTY(QString balanceText READ balanceText NOTIFY summaryChanged)
    // 金额隐藏（隐私模式）：打开后所有金额显示为 *
    Q_PROPERTY(bool privacyMode READ privacyMode WRITE setPrivacyMode NOTIFY privacyModeChanged)
    Q_PROPERTY(QString netWorthText READ netWorthText NOTIFY summaryChanged)
    Q_PROPERTY(QString dayAverageText READ dayAverageText NOTIFY summaryChanged)
    Q_PROPERTY(QString countText READ countText NOTIFY summaryChanged)
    Q_PROPERTY(QString budgetUsedText READ budgetUsedText NOTIFY summaryChanged)
    Q_PROPERTY(QString budgetRemainText READ budgetRemainText NOTIFY summaryChanged)
    Q_PROPERTY(QString budgetLimitText READ budgetLimitText NOTIFY summaryChanged)
    Q_PROPERTY(bool hasBudget READ hasBudget NOTIFY summaryChanged)
    Q_PROPERTY(bool budgetNearLimit READ budgetNearLimit NOTIFY summaryChanged)
    Q_PROPERTY(double budgetRatio READ budgetRatio NOTIFY summaryChanged)
    Q_PROPERTY(QString expenseDeltaText READ expenseDeltaText NOTIFY summaryChanged)
    Q_PROPERTY(QString incomeDeltaText READ incomeDeltaText NOTIFY summaryChanged)
    Q_PROPERTY(QString balanceRatioText READ balanceRatioText NOTIFY summaryChanged)
    Q_PROPERTY(int transactionCount READ transactionCount NOTIFY summaryChanged)
    Q_PROPERTY(bool hasActiveFilter READ hasActiveFilter NOTIFY summaryChanged)
    Q_PROPERTY(QStringList allTags READ allTags NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList recurringRules READ recurringRules NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList expenseCategoryTree READ expenseCategoryTree NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList incomeCategoryTree READ incomeCategoryTree NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList ledgers READ ledgers NOTIFY summaryChanged)
    Q_PROPERTY(int currentLedgerId READ currentLedgerId NOTIFY summaryChanged)
    Q_PROPERTY(QString currentLedgerName READ currentLedgerName NOTIFY summaryChanged)
    Q_PROPERTY(QString lastBackupText READ lastBackupText NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList managedCategories READ managedCategories NOTIFY summaryChanged)

    // 预算提醒：0 = 无，1 = 接近上限，2 = 已超支
    Q_PROPERTY(int budgetAlertLevel READ budgetAlertLevel NOTIFY summaryChanged)
    Q_PROPERTY(QString budgetAlertText READ budgetAlertText NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList categoryShares READ categoryShares NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList trend READ trend NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList accountTotals READ accountTotals NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList budgetRows READ budgetRows NOTIFY summaryChanged)
    Q_PROPERTY(bool budgetOverAllocated READ budgetOverAllocated NOTIFY summaryChanged)
    Q_PROPERTY(QString budgetWarningText READ budgetWarningText NOTIFY summaryChanged)
    Q_PROPERTY(QString budgetCategorySumText READ budgetCategorySumText NOTIFY summaryChanged)

    // 报表范围："month" | "3m" | "6m" | "year"
    Q_PROPERTY(QString reportRange READ reportRange NOTIFY reportChanged)
    Q_PROPERTY(QString reportRangeLabel READ reportRangeLabel NOTIFY reportChanged)
    Q_PROPERTY(QString reportIncomeText READ reportIncomeText NOTIFY reportChanged)
    Q_PROPERTY(QString reportExpenseText READ reportExpenseText NOTIFY reportChanged)
    Q_PROPERTY(QString reportBalanceText READ reportBalanceText NOTIFY reportChanged)
    Q_PROPERTY(QString reportCountText READ reportCountText NOTIFY reportChanged)
    Q_PROPERTY(QVariantList reportShares READ reportShares NOTIFY reportChanged)
    Q_PROPERTY(QVariantList reportAccountTotals READ reportAccountTotals NOTIFY reportChanged)
    Q_PROPERTY(QVariantList reportTrend READ reportTrend NOTIFY reportChanged)
    Q_PROPERTY(QVariantList reportMonthlyRows READ reportMonthlyRows NOTIFY reportChanged)
    Q_PROPERTY(QVariantList reportTagShares READ reportTagShares NOTIFY reportChanged)
    Q_PROPERTY(QString reportExpenseMoM READ reportExpenseMoM NOTIFY reportChanged)
    Q_PROPERTY(QString reportExpenseYoY READ reportExpenseYoY NOTIFY reportChanged)
    Q_PROPERTY(QString reportIncomeMoM READ reportIncomeMoM NOTIFY reportChanged)
    Q_PROPERTY(QVariantList expenseCategories READ expenseCategories NOTIFY summaryChanged)
    Q_PROPERTY(bool filterActive READ filterActive NOTIFY summaryChanged)
    Q_PROPERTY(int filteredCount READ filteredCount NOTIFY summaryChanged)
    Q_PROPERTY(QString filterSummary READ filterSummary NOTIFY summaryChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY undoStateChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY undoStateChanged)
    Q_PROPERTY(QString undoText READ undoText NOTIFY undoStateChanged)
    Q_PROPERTY(QString redoText READ redoText NOTIFY undoStateChanged)
    Q_PROPERTY(QVariantList incomeCategories READ incomeCategories NOTIFY summaryChanged)
    Q_PROPERTY(QAbstractItemModel *transactions READ transactions CONSTANT)
    Q_PROPERTY(QAbstractItemModel *accounts READ accounts CONSTANT)
    Q_PROPERTY(AccountListModel *accountModel READ accountModel CONSTANT)
    Q_PROPERTY(TransactionListModel *transactionModel READ transactionModel CONSTANT)

public:
    explicit LedgerViewModel(QSqlDatabase db, QObject *parent = nullptr);

    [[nodiscard]] QString incomeText() const;
    [[nodiscard]] QString expenseText() const;
    [[nodiscard]] QString balanceText() const;
    [[nodiscard]] bool privacyMode() const { return keep::Privacy::enabled(); }
    Q_INVOKABLE void setPrivacyMode(bool enabled);
    [[nodiscard]] QString netWorthText() const;
    [[nodiscard]] QString dayAverageText() const;
    [[nodiscard]] QString countText() const;
    [[nodiscard]] QString budgetUsedText() const;
    [[nodiscard]] QString budgetRemainText() const;
    [[nodiscard]] QString budgetLimitText() const;
    [[nodiscard]] bool hasBudget() const;
    [[nodiscard]] bool budgetNearLimit() const;
    [[nodiscard]] double budgetRatio() const;
    [[nodiscard]] QString expenseDeltaText() const;
    [[nodiscard]] QString incomeDeltaText() const;
    [[nodiscard]] QString balanceRatioText() const;
    [[nodiscard]] int transactionCount() const;
    [[nodiscard]] bool hasActiveFilter() const;
    [[nodiscard]] QStringList allTags() const { return m_allTagNames; }
    [[nodiscard]] QVariantList recurringRules() const { return m_recurringRules; }
    [[nodiscard]] QVariantList expenseCategoryTree() const { return m_expenseCategoryTree; }
    [[nodiscard]] QVariantList incomeCategoryTree() const { return m_incomeCategoryTree; }
    [[nodiscard]] QVariantList ledgers() const { return m_ledgers; }
    [[nodiscard]] int currentLedgerId() const { return static_cast<int>(m_currentLedgerId); }
    [[nodiscard]] QString currentLedgerName() const { return m_currentLedgerName; }
    [[nodiscard]] QString lastBackupText() const { return m_lastBackupText; }
    [[nodiscard]] QVariantList managedCategories() const { return m_managedCategories; }

    // 数据安全：立即备份 / 全量导出
    Q_INVOKABLE QVariantMap backupNow();
    Q_INVOKABLE QVariantMap exportAllJson(const QString &directory = QString());
    // 全量备份导出 / 导入（账本、账户、分类、流水、标签、预算、定期账单、设置）
    Q_INVOKABLE QVariantMap exportBackup(const QString &filePath);
    Q_INVOKABLE QVariantMap importBackup(const QString &filePath);

    // 分类管理
    // parentId > 0 时表示新建小类（kind 自动跟随父分类）
    Q_INVOKABLE QVariantMap addCategory(const QString &kind, const QString &name,
                                       const QString &color, int parentId = -1);
    Q_INVOKABLE QVariantMap updateCategory(int categoryId, const QString &name,
                                          const QString &color);
    Q_INVOKABLE QVariantMap archiveCategory(int categoryId, bool archived);

    // 记一笔面板用：某大类本月的支出合计（含子类小类）与该类预算
    Q_INVOKABLE QVariantMap categoryBudgetSummary(int categoryId);

    // 投资类账户（理财 / 股票 / 投资）：每日收益记录与修复
    Q_INVOKABLE bool isInvestmentAccount(int accountId) const;
    Q_INVOKABLE QVariantList dailyRecords(int accountId);
    Q_INVOKABLE QVariantMap investmentSummary(int accountId);
    // entry: accountId, dateText, mode("income"|"value"), incomeText, valueText,
    //        commissionText, note, recordId(0=新增)
    Q_INVOKABLE QVariantMap saveDailyRecord(const QVariantMap &entry);
    Q_INVOKABLE QVariantMap removeDailyRecord(qint64 recordId);

    // 多账本
    Q_INVOKABLE QVariantMap switchLedger(int ledgerId);
    Q_INVOKABLE QVariantMap addLedger(const QString &name);
    Q_INVOKABLE QVariantMap renameLedger(int ledgerId, const QString &name);
    Q_INVOKABLE QVariantMap removeLedger(int ledgerId);
    [[nodiscard]] int budgetAlertLevel() const { return m_budgetAlertLevel; }
    [[nodiscard]] QString budgetAlertText() const { return m_budgetAlertText; }

    // 统一录入（记一笔面板）：
    // kind=expense/income/transfer；categoryId 可指向大类或小类；
    // transferNature: transfer=本账本互转, expense=转给他人记作支出, lend=转给他人记作借出
    Q_INVOKABLE QVariantMap saveEntry(const QVariantMap &entry);
    Q_INVOKABLE QVariantMap updateEntry(qint64 id, const QVariantMap &entry);

    // 定期账单
    Q_INVOKABLE QVariantMap addRecurring(const QString &kind, const QString &amountText,
                                         int accountId, int categoryId,
                                         const QString &note, int dayOfMonth);
    Q_INVOKABLE QVariantMap updateRecurring(qint64 id, const QString &kind,
                                            const QString &amountText, int accountId,
                                            int categoryId, const QString &note,
                                            int dayOfMonth);
    Q_INVOKABLE QVariantMap removeRecurring(qint64 id);
    Q_INVOKABLE QVariantMap setRecurringEnabled(qint64 id, bool enabled);
    Q_INVOKABLE QVariantMap generateRecurring();

    // 主题持久化（settings.theme.dark）
    [[nodiscard]] Q_INVOKABLE bool themeDark() const;
    Q_INVOKABLE void saveThemeDark(bool dark);
    [[nodiscard]] QVariantList categoryShares() const;
    [[nodiscard]] QVariantList trend() const;
    [[nodiscard]] QVariantList accountTotals() const { return m_accountTotals; }

    [[nodiscard]] QVariantList budgetRows() const { return m_budgetRows; }

    [[nodiscard]] QString reportRange() const { return m_reportRange; }
    [[nodiscard]] QString reportRangeLabel() const { return m_reportRangeLabel; }
    [[nodiscard]] QString reportIncomeText() const { return m_reportIncomeText; }
    [[nodiscard]] QString reportExpenseText() const { return m_reportExpenseText; }
    [[nodiscard]] QString reportBalanceText() const { return m_reportBalanceText; }
    [[nodiscard]] QString reportCountText() const { return m_reportCountText; }
    [[nodiscard]] QVariantList reportShares() const { return m_reportShares; }
    [[nodiscard]] QVariantList reportAccountTotals() const { return m_reportAccountTotals; }
    [[nodiscard]] QVariantList reportTrend() const { return m_reportTrend; }
    [[nodiscard]] QVariantList reportMonthlyRows() const { return m_reportMonthlyRows; }
    [[nodiscard]] QVariantList reportTagShares() const { return m_reportTagShares; }
    [[nodiscard]] QString reportExpenseMoM() const { return m_reportExpenseMoM; }
    [[nodiscard]] QString reportExpenseYoY() const { return m_reportExpenseYoY; }
    [[nodiscard]] QString reportIncomeMoM() const { return m_reportIncomeMoM; }

    // 导出 PNG 等需要知道「文档」目录
    [[nodiscard]] Q_INVOKABLE QString documentsDirectory() const;

    // 设置页用
    [[nodiscard]] Q_INVOKABLE QString databasePath() const;
    Q_INVOKABLE void openDataFolder();
    [[nodiscard]] Q_INVOKABLE int totalTransactionCount() const;

    Q_INVOKABLE void setReportRange(const QString &range);

    [[nodiscard]] bool budgetOverAllocated() const { return m_budgetOverAllocated; }
    [[nodiscard]] QString budgetWarningText() const { return m_budgetWarningText; }
    [[nodiscard]] QString budgetCategorySumText() const { return m_budgetCategorySumText; }

    // 预算设置：amountText 为空或 0 表示清除
    Q_INVOKABLE QVariantMap setTotalBudget(const QString &amountText);
    Q_INVOKABLE QVariantMap setCategoryBudget(int categoryId, const QString &amountText);

    // 预算按月复制
    Q_INVOKABLE QVariantMap copyBudgetToNextMonth();
    Q_INVOKABLE QVariantMap copyBudgetFromPreviousMonth();

    // 账单导入：预览 / 执行（accountId<=0 或 categoryId<=0 表示未选）
    Q_INVOKABLE QVariantMap previewImportCsv(const QString &path);
    Q_INVOKABLE QVariantMap importCsv(const QString &path, int accountId, int categoryId);

    // 导出当前报表为 PDF（QPdfWriter，A4，含 KPI/分类/账户/标签/月度明细）
    Q_INVOKABLE QVariantMap exportReportPdf(const QString &directory = QString());

    // 导出当前月流水为 CSV（UTF-8 带 BOM，Excel 直接可读）；返回 {ok, message, path}
    Q_INVOKABLE QVariantMap exportMonthCsv(const QString &directory = QString());
    [[nodiscard]] QAbstractItemModel *transactions();
    [[nodiscard]] QAbstractItemModel *accounts();
    [[nodiscard]] AccountListModel *accountModel() { return &m_accountModel; }
    [[nodiscard]] TransactionListModel *transactionModel() { return &m_transactionModel; }

    [[nodiscard]] bool filterActive() const;
    [[nodiscard]] int filteredCount() const;
    [[nodiscard]] QString filterSummary() const;

    Q_INVOKABLE void setKeyword(const QString &keyword);
    Q_INVOKABLE void setAccountFilter(int accountId);   // -1 表示清除
    Q_INVOKABLE void setCategoryFilter(int categoryId); // -1 表示清除
    Q_INVOKABLE void clearFilter();

    [[nodiscard]] bool canUndo() const;
    [[nodiscard]] bool canRedo() const;
    [[nodiscard]] QString undoText() const;
    [[nodiscard]] QString redoText() const;

    [[nodiscard]] QVariantList expenseCategories() const { return m_expenseCategories; }
    [[nodiscard]] QVariantList incomeCategories() const { return m_incomeCategories; }

    Q_INVOKABLE void setMonth(int year, int month);
    Q_INVOKABLE void reload();

    // 写入一笔：kind 为 "expense"/"income"，金额用字符串避免浮点误差；返回 {ok, message, id}
    Q_INVOKABLE QVariantMap addTransaction(const QString &kind, const QString &amountText,
                                           int accountId, int categoryId,
                                           const QString &dateText, const QString &note,
                                           const QString &tags = QString());
    Q_INVOKABLE QVariantMap removeTransaction(qint64 id);
    // 覆盖式设置某一笔的标签（逗号/顿号/空格分隔；空字符串表示清空）
    Q_INVOKABLE QVariantMap setTransactionTags(qint64 id, const QString &tags);
    // ---- 账户管理 ----
    Q_INVOKABLE QVariantMap addAccount(const QString &name, const QString &type,
                                       const QString &openingBalanceText,
                                       const QString &creditLimitText);
    Q_INVOKABLE QVariantMap updateAccount(qint64 id, const QString &name, const QString &type,
                                          const QString &openingBalanceText,
                                          const QString &creditLimitText);
    Q_INVOKABLE QVariantMap archiveAccount(qint64 id, bool archived);
    Q_INVOKABLE QVariantMap accountDetail(qint64 id);

    Q_INVOKABLE QVariantMap updateTransaction(qint64 id, const QString &kind, const QString &amountText,
                                              int accountId, int categoryId, const QString &dateText,
                                              const QString &note, const QString &tags = QString());
    Q_INVOKABLE QVariantMap undo();
    Q_INVOKABLE QVariantMap redo();

    [[nodiscard]] Q_INVOKABLE int defaultAccountId() const;
    [[nodiscard]] Q_INVOKABLE int defaultCategoryId(const QString &kind) const;

signals:
    void summaryChanged();
    void privacyModeChanged();
    void reportChanged();
    void undoStateChanged();

private:
    void refreshSummary();

    QSqlDatabase m_db;
    AccountRepository m_accountRepository;
    AccountDailyRecordRepository m_dailyRecordRepository;
    BudgetRepository m_budgetRepository;
    CategoryRepository m_categoryRepository;
    TransactionRepository m_transactionRepository;
    TagRepository m_tagRepository;
    SettingsRepository m_settingsRepository;
    RecurringRepository m_recurringRepository;
    LedgerRepository m_ledgerRepository;

    TransactionListModel m_transactionModel;
    AccountListModel m_accountModel;

    int m_year = 0;
    int m_month = 0;

    MonthSummary m_summary;
    Money m_netWorth;
    bool m_hasBudget = false;
    bool m_budgetNearLimit = false;
    double m_budgetRatio = 0.0;
    QString m_budgetLimitText;
    QString m_budgetRemainText;
    QString m_budgetRatioText;
    QString m_expenseDeltaText;
    QString m_incomeDeltaText;
    QString m_balanceRatioText;
    QVariantList m_shares;
    QVariantList m_trend;
    QVariantList m_accountTotals;
    QVariantList m_budgetRows;
    QVariantList m_recurringRules;
    QVariantList m_ledgers;
    qint64 m_currentLedgerId = 0;
    QString m_currentLedgerName;

    void applyLedgerScope();
    bool m_budgetOverAllocated = false;
    QString m_budgetWarningText;
    QString m_budgetCategorySumText;

    QString m_reportRange = QStringLiteral("month");
    QString m_reportRangeLabel;
    QString m_reportIncomeText;
    QString m_reportExpenseText;
    QString m_reportBalanceText;
    QString m_reportCountText;
    QVariantList m_reportShares;
    QVariantList m_reportAccountTotals;
    QVariantList m_reportTrend;
    QVariantList m_reportMonthlyRows;
    QVariantList m_reportTagShares;
    QStringList m_allTagNames;
    QString m_lastBackupText;
    QVariantList m_managedCategories;
    QVariantList m_expenseCategoryTree;
    QVariantList m_incomeCategoryTree;
    int m_budgetAlertLevel = 0;
    QString m_budgetAlertText;
    QString m_reportExpenseMoM;
    QString m_reportExpenseYoY;
    QString m_reportIncomeMoM;

    void refreshReport();
    [[nodiscard]] QJsonObject buildFullExport() const;
    TransactionFilter m_filter;
    QUndoStack m_undoStack;
    QVariantList m_expenseCategories;
    QVariantList m_incomeCategories;
};

}  // namespace keep