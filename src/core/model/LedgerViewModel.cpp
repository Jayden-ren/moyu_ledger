#include "core/model/LedgerViewModel.h"

#include "core/data/BackupService.h"
#include "core/data/ImportService.h"

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>

#include <cstdlib>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QDate>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QDesktopServices>
#include <QFileInfo>
#include <QPainter>
#include <QPdfWriter>
#include <QPageSize>
#include <QStandardPaths>
#include <QUrl>
#include <QTextStream>
#include <QUndoCommand>

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <QHash>
#include <QPair>
#include <QString>

namespace keep {

namespace {

// 轻量命令包装：动作先执行，再压栈（首次 redo 跳过，避免重复执行）
class FunctionCommand : public QUndoCommand
{
public:
    FunctionCommand(std::function<void()> redoFn, std::function<void()> undoFn, const QString &text)
        : QUndoCommand(text)
        , m_redo(std::move(redoFn))
        , m_undo(std::move(undoFn))
    {
    }

    void redo() override
    {
        if (m_skipFirst) {
            m_skipFirst = false;
            return;
        }
        m_redo();
    }

    void undo() override { m_undo(); }

private:
    std::function<void()> m_redo;
    std::function<void()> m_undo;
    bool m_skipFirst = true;
};

QString percentText(double share)
{
    return QStringLiteral("%1%").arg(QString::number(share * 100.0, 'f', 1));
}
}  // namespace

LedgerViewModel::LedgerViewModel(QSqlDatabase db, QObject *parent)
    : QObject(parent)
    , m_db(db)
    , m_accountRepository(db)
    , m_dailyRecordRepository(db)
    , m_budgetRepository(db)
    , m_categoryRepository(db)
    , m_transactionRepository(db)
    , m_tagRepository(db)
    , m_settingsRepository(db)
    , m_recurringRepository(db)
    , m_ledgerRepository(db)
{
    // 恢复上次使用的账本（settings.ledger.current），否则用默认账本；
    // 作用域必须在首次 reload 之前设置
    const QString savedLedger = m_settingsRepository.value(QStringLiteral("ledger.current"));
    bool restoredLedger = false;
    if (!savedLedger.isEmpty()) {
        const qint64 candidate = savedLedger.toLongLong();
        for (const Ledger &ledger : m_ledgerRepository.listAll()) {
            if (ledger.id == candidate) {
                m_currentLedgerId = candidate;
                restoredLedger = true;
                break;
            }
        }
    }
    if (!restoredLedger) {
        if (const auto defaultLedger = m_ledgerRepository.defaultId(); defaultLedger.has_value())
            m_currentLedgerId = *defaultLedger;
    }
    applyLedgerScope();

    // 首次运行（空库）播种内置分类：干净版本必须开箱就能分类记账
    if (m_categoryRepository.count() == 0)
        m_categoryRepository.seedBuiltins();
    m_categoryRepository.seedSubcategories();
    m_categoryRepository.seedInvestmentCategories();

    connect(&m_undoStack, &QUndoStack::indexChanged, this, [this] { emit undoStateChanged(); });
    connect(&m_undoStack, &QUndoStack::canUndoChanged, this, [this] { emit undoStateChanged(); });
    connect(&m_undoStack, &QUndoStack::canRedoChanged, this, [this] { emit undoStateChanged(); });

    const QDate today = QDate::currentDate();
    m_year = today.year();
    m_month = today.month();
    reload();
}

bool LedgerViewModel::filterActive() const { return !m_filter.isEmpty(); }
int LedgerViewModel::filteredCount() const { return m_transactionModel.count(); }

QString LedgerViewModel::filterSummary() const
{
    if (!filterActive())
        return QString();

    QStringList parts;
    if (!m_filter.keyword.trimmed().isEmpty())
        parts << QStringLiteral("关键字「%1」").arg(m_filter.keyword.trimmed());
    if (m_filter.accountId.has_value()) {
        const auto account = m_accountRepository.findById(*m_filter.accountId);
        if (account.has_value())
            parts << QStringLiteral("账户 %1").arg(account->name);
    }
    if (m_filter.categoryId.has_value()) {
        const auto category = m_categoryRepository.findById(*m_filter.categoryId);
        if (category.has_value())
            parts << QStringLiteral("分类 %1").arg(category->name);
    }
    return parts.join(QStringLiteral(" · "));
}

void LedgerViewModel::setKeyword(const QString &keyword)
{
    if (m_filter.keyword == keyword)
        return;
    m_filter.keyword = keyword;
    reload();
}

void LedgerViewModel::setAccountFilter(int accountId)
{
    const std::optional<qint64> value = accountId > 0 ? std::optional<qint64>(accountId)
                                                      : std::nullopt;
    if (m_filter.accountId == value)
        return;
    m_filter.accountId = value;
    reload();
}

void LedgerViewModel::setCategoryFilter(int categoryId)
{
    const std::optional<qint64> value = categoryId > 0 ? std::optional<qint64>(categoryId)
                                                       : std::nullopt;
    if (m_filter.categoryId == value)
        return;
    m_filter.categoryId = value;
    reload();
}

void LedgerViewModel::clearFilter()
{
    if (m_filter.isEmpty())
        return;
    m_filter = TransactionFilter{};
    reload();
}

bool LedgerViewModel::hasActiveFilter() const { return !m_filter.isEmpty(); }

bool LedgerViewModel::canUndo() const { return m_undoStack.canUndo(); }
bool LedgerViewModel::canRedo() const { return m_undoStack.canRedo(); }
QString LedgerViewModel::undoText() const { return m_undoStack.undoText(); }
QString LedgerViewModel::redoText() const { return m_undoStack.redoText(); }

QVariantMap LedgerViewModel::undo()
{
    QVariantMap result;
    const QString text = m_undoStack.undoText();
    m_undoStack.undo();
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"),
                  text.isEmpty() ? QStringLiteral("没有可撤销的操作")
                                 : QStringLiteral("已撤销：%1").arg(text));
    return result;
}

QVariantMap LedgerViewModel::redo()
{
    QVariantMap result;
    const QString text = m_undoStack.redoText();
    m_undoStack.redo();
    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"),
                  text.isEmpty() ? QStringLiteral("没有可重做的操作")
                                 : QStringLiteral("已重做：%1").arg(text));
    return result;
}

QString LedgerViewModel::incomeText() const { return m_summary.income.format(); }
QString LedgerViewModel::expenseText() const { return m_summary.expense.format(); }
void LedgerViewModel::setPrivacyMode(bool enabled)
{
    if (keep::Privacy::enabled() == enabled)
        return;
    keep::Privacy::setEnabled(enabled);
    // 金额文本都在 reload 里生成，重新加载即可整体刷新
    reload();
    emit privacyModeChanged();
    emit summaryChanged();
    emit reportChanged();
}

QString LedgerViewModel::balanceText() const { return m_summary.balance().format(); }
QString LedgerViewModel::netWorthText() const { return m_netWorth.format(); }
QString LedgerViewModel::countText() const { return QString::number(m_summary.count); }
QString LedgerViewModel::budgetUsedText() const
{
    return m_hasBudget ? m_budgetRatioText : QStringLiteral("—");
}
QString LedgerViewModel::budgetRemainText() const
{
    return m_hasBudget ? m_budgetRemainText : QStringLiteral("尚未设置预算");
}
QString LedgerViewModel::budgetLimitText() const
{
    return m_hasBudget ? m_budgetLimitText : QStringLiteral("—");
}
bool LedgerViewModel::hasBudget() const { return m_hasBudget; }
bool LedgerViewModel::budgetNearLimit() const { return m_budgetNearLimit; }
double LedgerViewModel::budgetRatio() const { return m_budgetRatio; }
QString LedgerViewModel::expenseDeltaText() const { return m_expenseDeltaText; }
QString LedgerViewModel::incomeDeltaText() const { return m_incomeDeltaText; }
QString LedgerViewModel::balanceRatioText() const { return m_balanceRatioText; }
int LedgerViewModel::transactionCount() const { return m_summary.count; }
QVariantList LedgerViewModel::categoryShares() const { return m_shares; }
QVariantList LedgerViewModel::trend() const { return m_trend; }
QAbstractItemModel *LedgerViewModel::transactions() { return &m_transactionModel; }
QAbstractItemModel *LedgerViewModel::accounts() { return &m_accountModel; }

QString LedgerViewModel::dayAverageText() const
{
    const QDate today = QDate::currentDate();
    const bool currentMonth = (today.year() == m_year && today.month() == m_month);
    const int days = currentMonth ? std::max(1, today.day())
                                  : QDate(m_year, m_month, 1).daysInMonth();
    if (m_summary.expense.isZero())
        return QStringLiteral("¥0");
    return Money::fromCents(m_summary.expense.cents() / days).format();
}

void LedgerViewModel::setMonth(int year, int month)
{
    if (year == m_year && month == m_month)
        return;
    m_year = year;
    m_month = month;
    reload();
}

void LedgerViewModel::reload()
{
    if (m_year == 0 || m_month == 0) {
        const QDate today = QDate::currentDate();
        m_year = today.year();
        m_month = today.month();
    }

    m_summary = m_transactionRepository.monthSummary(m_year, m_month);

    const auto accountRows = m_accountRepository.listWithBalances();
    const auto netWorth = m_accountRepository.netWorth();
    m_netWorth = netWorth.value_or(Money());

    QHash<qint64, QString> accountNames;
    for (const AccountWithBalance &row : accountRows)
        accountNames.insert(row.account.id, row.account.name);

    const QVector<Category> categories = m_categoryRepository.listAll(true);
    QHash<qint64, QString> categoryNames;
    QHash<qint64, QPair<QString, QString>> categoryBadges;
    for (const Category &category : categories) {
        categoryNames.insert(category.id, category.name);
        categoryBadges.insert(category.id,
                              {category.icon.isEmpty() ? category.name.left(1) : category.icon,
                               category.color});
    }

    const QVector<Transaction> monthTransactions =
        m_transactionRepository.listByMonth(m_year, m_month, m_filter);
    QVector<qint64> transactionIds;
    transactionIds.reserve(monthTransactions.size());
    for (const Transaction &row : monthTransactions)
        transactionIds.append(row.id);
    const QHash<qint64, QStringList> tagLists = m_tagRepository.tagsForTransactions(transactionIds);
    QHash<qint64, QString> tagTexts;
    for (auto it = tagLists.cbegin(); it != tagLists.cend(); ++it)
        tagTexts.insert(it.key(), it.value().join(QStringLiteral("、")));

    m_transactionModel.setTransactions(monthTransactions, accountNames, categoryNames,
                                       categoryBadges, tagTexts);
    m_accountModel.setAccounts(accountRows);

    QVariantList shares;
    for (const CategoryTotal &total : m_transactionRepository.categoryShares(m_year, m_month)) {
        QVariantMap item;
        item.insert(QStringLiteral("name"), total.name);
        item.insert(QStringLiteral("color"), total.color);
        item.insert(QStringLiteral("totalText"), total.total.format());
        item.insert(QStringLiteral("shareText"), percentText(total.share));
        item.insert(QStringLiteral("share"), total.share);
        shares.append(item);
    }
    m_shares = shares;

    QVariantList trend;
    for (const MonthPoint &point : m_transactionRepository.monthlyTrend(m_year, m_month, 6)) {
        QVariantMap item;
        item.insert(QStringLiteral("label"), QStringLiteral("%1月").arg(point.month));
        item.insert(QStringLiteral("expenseCents"), point.expense.cents());
        item.insert(QStringLiteral("expenseText"), point.expense.format());
        item.insert(QStringLiteral("incomeCents"), point.income.cents());
        trend.append(item);
    }
    m_trend = trend;

    // 账户维度汇总（报表页用）
    QVariantList accountTotals;
    for (const AccountTotal &total : m_transactionRepository.accountTotals(m_year, m_month)) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), total.accountId);
        item.insert(QStringLiteral("name"), total.name);
        item.insert(QStringLiteral("color"), total.color);
        item.insert(QStringLiteral("incomeText"), total.income.format());
        item.insert(QStringLiteral("expenseText"), total.expense.format());
        item.insert(QStringLiteral("count"), total.count);
        accountTotals.append(item);
    }
    m_accountTotals = accountTotals;

    // 分类预算执行情况（预算页用）
    const QString monthKey = QStringLiteral("%1-%2")
                                 .arg(m_year)
                                 .arg(m_month, 2, 10, QLatin1Char('0'));
    QHash<qint64, Money> spentByCategory;
    for (const CategoryTotal &total : m_transactionRepository.categoryShares(m_year, m_month))
        spentByCategory.insert(total.categoryId, total.total);

    QHash<qint64, Money> budgetByCategory;
    for (const BudgetRepository::CategoryBudget &row : m_budgetRepository.categoryBudgets(monthKey))
        budgetByCategory.insert(row.categoryId, row.amount);

    QVariantList budgetRows;
    for (const Category &category : categories) {
        if (category.archived || category.kind != CategoryKind::Expense)
            continue;

        const Money spent = spentByCategory.value(category.id);
        const Money budgetAmount = budgetByCategory.value(category.id);
        const bool hasBudget = budgetAmount.cents() > 0;
        const double ratio = hasBudget
                                 ? static_cast<double>(spent.cents())
                                       / static_cast<double>(budgetAmount.cents())
                                 : 0.0;

        QVariantMap item;
        item.insert(QStringLiteral("categoryId"), category.id);
        item.insert(QStringLiteral("name"), category.name);
        item.insert(QStringLiteral("color"), category.color);
        item.insert(QStringLiteral("badge"),
                    category.icon.isEmpty() ? category.name.left(1) : category.icon);
        item.insert(QStringLiteral("hasBudget"), hasBudget);
        item.insert(QStringLiteral("budgetText"), hasBudget ? budgetAmount.format() : QStringLiteral("未设置"));
        item.insert(QStringLiteral("budgetInput"), hasBudget ? budgetAmount.format(false) : QString());
        item.insert(QStringLiteral("spentText"), spent.format());
        item.insert(QStringLiteral("remainText"),
                    hasBudget ? Money(budgetAmount.cents() - spent.cents()).format()
                              : QStringLiteral("—"));
        item.insert(QStringLiteral("ratio"), ratio);
        item.insert(QStringLiteral("ratioText"),
                    hasBudget ? QStringLiteral("%1%").arg(QString::number(ratio * 100.0, 'f', 1))
                              : QString());
        item.insert(QStringLiteral("overLimit"), hasBudget && spent.cents() > budgetAmount.cents());
        budgetRows.append(item);
    }
    m_budgetRows = budgetRows;

    // 两级分类树（大类 -> 小类），供记一笔面板使用
    {
        struct TreeNode
        {
            qint64 id = 0;
            QVariantMap item;
            QVariantList children;
        };
        QVector<TreeNode> bigNodes;
        QHash<qint64, int> bigIndex;

        for (const Category &category : m_categoryRepository.listAll(false)) {
            if (!category.parentId.has_value()) {
                TreeNode node;
                node.id = category.id;
                QVariantMap item;
                item.insert(QStringLiteral("id"), category.id);
                item.insert(QStringLiteral("name"), category.name);
                item.insert(QStringLiteral("color"), category.color);
                item.insert(QStringLiteral("icon"),
                            category.icon.isEmpty() ? category.name.left(1) : category.icon);
                item.insert(QStringLiteral("kind"), toDbString(category.kind));
                item.insert(QStringLiteral("parentId"), qint64(0));
                node.item = item;
                bigIndex.insert(category.id, bigNodes.size());
                bigNodes.append(node);
            }
        }
        for (const Category &category : m_categoryRepository.listAll(false)) {
            if (!category.parentId.has_value())
                continue;
            const auto index = bigIndex.constFind(*category.parentId);
            if (index == bigIndex.cend())
                continue;
            QVariantMap child;
            child.insert(QStringLiteral("id"), category.id);
            child.insert(QStringLiteral("name"), category.name);
            child.insert(QStringLiteral("color"), category.color);
            child.insert(QStringLiteral("parentId"), *category.parentId);
            bigNodes[index.value()].children.append(child);
        }

        QVariantList expenseTree;
        QVariantList incomeTree;
        for (const TreeNode &node : bigNodes) {
            QVariantMap item = node.item;
            item.insert(QStringLiteral("children"), node.children);
            const QString kind = item.value(QStringLiteral("kind")).toString();
            if (kind == QLatin1String("income"))
                incomeTree.append(item);
            else
                expenseTree.append(item);
        }
        m_expenseCategoryTree = expenseTree;
        m_incomeCategoryTree = incomeTree;
    }

    // 备份状态 + 分类管理列表
    {
        const QDateTime lastBackup = BackupService::lastBackupTime(m_db.databaseName());
        m_lastBackupText = lastBackup.isValid()
                               ? lastBackup.toString(QStringLiteral("yyyy-MM-dd HH:mm"))
                               : QStringLiteral("从未备份");
    }

    QVariantList managedCategories;
    for (const Category &category : m_categoryRepository.listAll(true)) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), category.id);
        item.insert(QStringLiteral("name"), category.name);
        item.insert(QStringLiteral("kind"),
                    category.kind == CategoryKind::Income ? QStringLiteral("income")
                                                          : QStringLiteral("expense"));
        item.insert(QStringLiteral("kindText"), displayName(category.kind));
        item.insert(QStringLiteral("color"), category.color);
        item.insert(QStringLiteral("icon"), category.icon);
        item.insert(QStringLiteral("archived"), category.archived);
        item.insert(QStringLiteral("builtin"), category.builtin);
        managedCategories.append(item);
    }
    m_managedCategories = managedCategories;

    // 账本列表
    const QVector<Ledger> ledgerRows = m_ledgerRepository.listAll();
    QVariantList ledgers;
    for (const Ledger &ledger : ledgerRows) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), ledger.id);
        item.insert(QStringLiteral("name"), ledger.name);
        item.insert(QStringLiteral("color"), ledger.color);
        item.insert(QStringLiteral("isDefault"), ledger.isDefault);
        item.insert(QStringLiteral("accountCount"), ledger.accountCount);
        item.insert(QStringLiteral("isCurrent"), ledger.id == m_currentLedgerId);
        ledgers.append(item);
        if (ledger.id == m_currentLedgerId)
            m_currentLedgerName = ledger.name;
    }
    m_ledgers = ledgers;

    // 定期账单列表（设置页用）
    QHash<qint64, QString> accountNameById;
    for (const AccountWithBalance &row : accountRows)
        accountNameById.insert(row.account.id, row.account.name);

    QVariantList recurringRules;
    for (const RecurringRule &rule : m_recurringRepository.listAll()) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), rule.id);
        item.insert(QStringLiteral("kind"),
                    rule.kind == TransactionKind::Income ? QStringLiteral("income")
                                                         : QStringLiteral("expense"));
        item.insert(QStringLiteral("kindText"), displayName(rule.kind));
        item.insert(QStringLiteral("amountText"), rule.amount.format());
        item.insert(QStringLiteral("amountInput"), rule.amount.format(false));
        item.insert(QStringLiteral("accountId"), rule.accountId);
        item.insert(QStringLiteral("accountName"),
                    accountNameById.value(rule.accountId));
        item.insert(QStringLiteral("categoryId"),
                    rule.categoryId.has_value() ? *rule.categoryId : qint64(-1));
        item.insert(QStringLiteral("categoryName"),
                    rule.categoryId.has_value()
                        ? categoryNames.value(*rule.categoryId)
                        : QString());
        item.insert(QStringLiteral("note"), rule.note);
        item.insert(QStringLiteral("dayOfMonth"), rule.dayOfMonth);
        item.insert(QStringLiteral("enabled"), rule.enabled);
        item.insert(QStringLiteral("lastGeneratedMonth"), rule.lastGeneratedMonth);
        recurringRules.append(item);
    }
    m_recurringRules = recurringRules;

    m_allTagNames = m_tagRepository.allTagNames();

    // 分类预算合计与超配提示
    Money categorySum;
    for (auto it = budgetByCategory.cbegin(); it != budgetByCategory.cend(); ++it)
        categorySum += it.value();
    m_budgetCategorySumText = categorySum.format();

    const auto totalLimit = m_budgetRepository.monthlyLimit(monthKey);
    const bool hasTotal = totalLimit.has_value() && totalLimit->cents() > 0;
    m_budgetOverAllocated = hasTotal && categorySum.cents() > totalLimit->cents();
    if (m_budgetOverAllocated) {
        m_budgetWarningText = QStringLiteral("分类预算合计 %1 已超过总预算 %2，超出 %3")
                                  .arg(categorySum.format(), totalLimit->format(),
                                       Money(categorySum.cents() - totalLimit->cents()).format());
    } else {
        m_budgetWarningText.clear();
    }

    refreshReport();

    QVariantList expenseList;
    QVariantList incomeList;
    for (const Category &category : categories) {
        if (category.archived)
            continue;
        QVariantMap item;
        item.insert(QStringLiteral("id"), category.id);
        item.insert(QStringLiteral("name"), category.name);
        item.insert(QStringLiteral("color"), category.color);
        item.insert(QStringLiteral("badge"), category.icon.isEmpty() ? category.name.left(1) : category.icon);
        if (category.kind == CategoryKind::Income)
            incomeList.append(item);
        else
            expenseList.append(item);
    }
    m_expenseCategories = expenseList;
    m_incomeCategories = incomeList;

    // ---- 预算执行 ----
    const auto limit = m_budgetRepository.monthlyLimit(monthKey);
    const qint64 budgetLimitCents = limit.has_value() ? limit->cents() : 0;
    m_hasBudget = limit.has_value() && limit->cents() > 0;
    if (m_hasBudget) {
        m_budgetLimitText = limit->format();
        m_budgetRatio = static_cast<double>(m_summary.expense.cents())
                        / static_cast<double>(limit->cents());
        m_budgetRemainText = Money(limit->cents() - m_summary.expense.cents()).format();
        m_budgetRatioText = QStringLiteral("%1%").arg(QString::number(m_budgetRatio * 100.0, 'f', 1));
        m_budgetNearLimit = m_budgetRatio >= 0.85;
    } else {
        m_budgetLimitText.clear();
        m_budgetRemainText.clear();
        m_budgetRatioText.clear();
        m_budgetRatio = 0.0;
        m_budgetNearLimit = false;
    }

    // ---- 环比与结余率 ----
    const QDate currentMonth(m_year, m_month, 1);
    const QDate previousMonth = currentMonth.addMonths(-1);
    const MonthSummary previous = m_transactionRepository.monthSummary(previousMonth.year(),
                                                                      previousMonth.month());
    const auto deltaText = [&previousMonth](Money now, Money before) -> QString {
        if (before.isZero())
            return QString();
        const double percent = (static_cast<double>(now.cents()) - static_cast<double>(before.cents()))
                               / static_cast<double>(before.cents()) * 100.0;
        const QString arrow = percent >= 0.0 ? QStringLiteral("↑") : QStringLiteral("↓");
        return QStringLiteral("%1 %2% 较 %3 月")
            .arg(arrow, QString::number(std::abs(percent), 'f', 1))
            .arg(previousMonth.month());
    };
    // 预算提醒：超支 / 接近上限
    if (m_hasBudget && m_summary.expense.cents() > budgetLimitCents) {
        m_budgetAlertLevel = 2;
        m_budgetAlertText = QStringLiteral("本月预算已超支 %1，已用 %2")
                                 .arg(Money(m_summary.expense.cents() - budgetLimitCents).format(),
                                      m_budgetRatioText);
    } else if (m_hasBudget && m_budgetRatio >= 0.85) {
        m_budgetAlertLevel = 1;
        m_budgetAlertText = QStringLiteral("本月预算已用 %1，剩余 %2")
                                 .arg(m_budgetRatioText, m_budgetRemainText);
    } else {
        m_budgetAlertLevel = 0;
        m_budgetAlertText.clear();
    }

    m_expenseDeltaText = deltaText(m_summary.expense, previous.expense);
    m_incomeDeltaText = deltaText(m_summary.income, previous.income);
    if (m_expenseDeltaText.isEmpty())
        m_expenseDeltaText = QStringLiteral("—");
    if (m_incomeDeltaText.isEmpty())
        m_incomeDeltaText = QStringLiteral("—");
    m_balanceRatioText =
        m_summary.income.isZero()
            ? QString()
            : QStringLiteral("结余率 %1%")
                  .arg(QString::number(static_cast<double>(m_summary.balance().cents())
                                           / static_cast<double>(m_summary.income.cents()) * 100.0,
                                       'f', 1));

    emit summaryChanged();
}

int LedgerViewModel::defaultAccountId() const
{
    const QVector<AccountWithBalance> rows = m_accountRepository.listWithBalances();
    return rows.isEmpty() ? -1 : static_cast<int>(rows.first().account.id);
}

int LedgerViewModel::defaultCategoryId(const QString &kind) const
{
    const QVariantList &list = (kind == QLatin1String("income")) ? m_incomeCategories : m_expenseCategories;
    return list.isEmpty() ? -1 : list.first().toMap().value(QStringLiteral("id")).toInt();
}

namespace {

QString csvField(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    if (value.contains(QLatin1Char(',')) || value.contains(QLatin1Char('"'))
        || value.contains(QLatin1Char('\n'))) {
        return QStringLiteral("\"") + value + QStringLiteral("\"");
    }
    return value;
}

}  // namespace

namespace {

// 把「餐饮,商务 报销」这类输入拆成标签列表（支持中英文逗号、顿号、分号、空格）
QStringList splitTagText(const QString &text)
{
    static const QRegularExpression separator(QStringLiteral("[,，、;；\\s]+"));
    QStringList names;
    for (const QString &piece : text.split(separator, Qt::SkipEmptyParts)) {
        const QString trimmed = piece.trimmed();
        if (!trimmed.isEmpty())
            names.append(trimmed);
    }
    return names;
}

bool parseOptionalAmount(const QString &text, Money *out)
{
    if (text.trimmed().isEmpty()) {
        *out = Money();
        return true;
    }
    const auto parsed = Money::fromYuan(text);
    if (!parsed.has_value())
        return false;
    *out = *parsed;
    return true;
}

QVariantMap budgetResult(bool ok, const QString &message)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("message"), message);
    return result;
}

}  // namespace

QVariantMap LedgerViewModel::copyBudgetToNextMonth()
{
    const QDate current(m_year, m_month, 1);
    const QDate target = current.addMonths(1);
    const QString fromKey = QStringLiteral("%1-%2").arg(m_year).arg(m_month, 2, 10, QLatin1Char('0'));
    const QString toKey = QStringLiteral("%1-%2")
                              .arg(target.year())
                              .arg(target.month(), 2, 10, QLatin1Char('0'));

    QString error;
    int copied = 0;

    const auto limit = m_budgetRepository.monthlyLimit(fromKey, &error);
    if (limit.has_value() && limit->cents() > 0) {
        if (!m_budgetRepository.setMonthlyLimit(toKey, *limit, &error))
            return budgetResult(false, error.isEmpty() ? QStringLiteral("复制失败") : error);
        ++copied;
    }

    for (const BudgetRepository::CategoryBudget &row : m_budgetRepository.categoryBudgets(fromKey)) {
        if (m_budgetRepository.setCategoryBudget(toKey, row.categoryId, row.amount, &error))
            ++copied;
    }

    reload();
    return budgetResult(copied > 0,
                        copied > 0 ? QStringLiteral("已复制 %1 项预算到 %2").arg(copied).arg(toKey)
                                   : QStringLiteral("本月没有可复制的预算"));
}

QVariantMap LedgerViewModel::copyBudgetFromPreviousMonth()
{
    const QDate current(m_year, m_month, 1);
    const QDate source = current.addMonths(-1);
    const QString toKey = QStringLiteral("%1-%2").arg(m_year).arg(m_month, 2, 10, QLatin1Char('0'));
    const QString fromKey = QStringLiteral("%1-%2")
                                .arg(source.year())
                                .arg(source.month(), 2, 10, QLatin1Char('0'));

    QString error;
    int copied = 0;

    const auto limit = m_budgetRepository.monthlyLimit(fromKey, &error);
    if (limit.has_value() && limit->cents() > 0) {
        if (!m_budgetRepository.setMonthlyLimit(toKey, *limit, &error))
            return budgetResult(false, error.isEmpty() ? QStringLiteral("复制失败") : error);
        ++copied;
    }

    for (const BudgetRepository::CategoryBudget &row : m_budgetRepository.categoryBudgets(fromKey)) {
        if (m_budgetRepository.setCategoryBudget(toKey, row.categoryId, row.amount, &error))
            ++copied;
    }

    reload();
    return budgetResult(copied > 0,
                        copied > 0 ? QStringLiteral("已从 %1 复制 %2 项预算").arg(fromKey).arg(copied)
                                   : QStringLiteral("上月没有设置预算"));
}

QVariantMap LedgerViewModel::setTotalBudget(const QString &amountText)
{
    Money amount;
    if (!parseOptionalAmount(amountText, &amount)) {
        return budgetResult(false, QStringLiteral("金额格式不正确"));
    }

    const QString monthKey = QStringLiteral("%1-%2")
                                 .arg(m_year)
                                 .arg(m_month, 2, 10, QLatin1Char('0'));
    QString error;
    if (!m_budgetRepository.setMonthlyLimit(monthKey, amount, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("保存失败") : error);

    reload();
    return budgetResult(true, amount.cents() > 0 ? QStringLiteral("本月总预算已保存")
                                                : QStringLiteral("已清除本月总预算"));
}

QVariantMap LedgerViewModel::setCategoryBudget(int categoryId, const QString &amountText)
{
    if (categoryId <= 0)
        return budgetResult(false, QStringLiteral("分类无效"));

    Money amount;
    if (!parseOptionalAmount(amountText, &amount)) {
        return budgetResult(false, QStringLiteral("金额格式不正确"));
    }

    const QString monthKey = QStringLiteral("%1-%2")
                                 .arg(m_year)
                                 .arg(m_month, 2, 10, QLatin1Char('0'));
    QString error;
    if (!m_budgetRepository.setCategoryBudget(monthKey, categoryId, amount, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("保存失败") : error);

    reload();
    return budgetResult(true, amount.cents() > 0 ? QStringLiteral("分类预算已保存")
                                                : QStringLiteral("已清除分类预算"));
}

void LedgerViewModel::setReportRange(const QString &range)
{
    static const QStringList allowed{QStringLiteral("month"), QStringLiteral("3m"),
                                     QStringLiteral("6m"), QStringLiteral("year")};
    if (!allowed.contains(range) || m_reportRange == range)
        return;
    m_reportRange = range;
    refreshReport();
    emit reportChanged();
}

void LedgerViewModel::refreshReport()
{
    int months = 1;
    if (m_reportRange == QLatin1String("3m"))
        months = 3;
    else if (m_reportRange == QLatin1String("6m"))
        months = 6;
    else if (m_reportRange == QLatin1String("year"))
        months = m_month;  // 1 月到当前月

    const QDate current(m_year, m_month, 1);
    QVector<QPair<int, int>> window;
    for (int i = months - 1; i >= 0; --i) {
        const QDate month = current.addMonths(-i);
        window.append(qMakePair(month.year(), month.month()));
    }

    MonthSummary total;
    QHash<qint64, double> shareAmounts;
    QHash<qint64, QString> shareNames;
    QHash<qint64, QString> shareColors;
    QHash<qint64, AccountTotal> accountMap;
    QVariantList trend;

    for (const auto &entry : window) {
        const MonthSummary summary = m_transactionRepository.monthSummary(entry.first, entry.second);
        total.income += summary.income;
        total.expense += summary.expense;
        total.count += summary.count;

        for (const CategoryTotal &row :
             m_transactionRepository.categoryShares(entry.first, entry.second)) {
            shareAmounts[row.categoryId] += static_cast<double>(row.total.cents());
            shareNames.insert(row.categoryId, row.name);
            shareColors.insert(row.categoryId, row.color);
        }

        for (const AccountTotal &row : m_transactionRepository.accountTotals(entry.first, entry.second)) {
            AccountTotal &accumulated = accountMap[row.accountId];
            accumulated.accountId = row.accountId;
            accumulated.name = row.name;
            accumulated.color = row.color;
            accumulated.income += row.income;
            accumulated.expense += row.expense;
            accumulated.count += row.count;
        }

        QVariantMap point;
        point.insert(QStringLiteral("label"),
                     QStringLiteral("%1月").arg(entry.second));
        point.insert(QStringLiteral("expenseCents"), summary.expense.cents());
        point.insert(QStringLiteral("expenseText"), summary.expense.format());
        point.insert(QStringLiteral("incomeCents"), summary.income.cents());
        trend.append(point);
    }

    // ---- 月度明细（按范围内每月一行）----
    QVariantList monthlyRows;
    for (const auto &entry : window) {
        const MonthSummary summary = m_transactionRepository.monthSummary(entry.first, entry.second);
        QVariantMap row;
        row.insert(QStringLiteral("label"), QStringLiteral("%1-%2")
                                                .arg(entry.first)
                                                .arg(entry.second, 2, 10, QLatin1Char('0')));
        row.insert(QStringLiteral("incomeText"), summary.income.format());
        row.insert(QStringLiteral("expenseText"), summary.expense.format());
        row.insert(QStringLiteral("balanceText"), summary.balance().format());
        row.insert(QStringLiteral("count"), summary.count);
        monthlyRows.append(row);
    }
    m_reportMonthlyRows = monthlyRows;

    // ---- 环比 / 同比 ----
    const auto rangeTotal = [this](const QVector<QPair<int, int>> &months) {
        MonthSummary sum;
        for (const auto &entry : months) {
            const MonthSummary one = m_transactionRepository.monthSummary(entry.first, entry.second);
            sum.income += one.income;
            sum.expense += one.expense;
        }
        return sum;
    };

    QVector<QPair<int, int>> previousWindow;
    QVector<QPair<int, int>> lastYearWindow;
    for (const auto &entry : window) {
        const QDate month(entry.first, entry.second, 1);
        const QDate previous = month.addMonths(-months);
        const QDate lastYear = month.addMonths(-12);
        previousWindow.append(qMakePair(previous.year(), previous.month()));
        lastYearWindow.append(qMakePair(lastYear.year(), lastYear.month()));
    }

    const MonthSummary previousTotal = rangeTotal(previousWindow);
    const MonthSummary lastYearTotal = rangeTotal(lastYearWindow);

    const auto deltaText = [](const Money &now, const Money &before) -> QString {
        if (before.isZero() || now.isZero() && before.isZero())
            return QStringLiteral("—");
        const double percent = (static_cast<double>(now.cents()) - static_cast<double>(before.cents()))
                               / static_cast<double>(before.cents()) * 100.0;
        const QString arrow = percent >= 0.0 ? QStringLiteral("↑") : QStringLiteral("↓");
        return arrow + QStringLiteral(" ") + QString::number(std::abs(percent), 'f', 1)
               + QStringLiteral("%");
    };

    m_reportExpenseMoM = deltaText(total.expense, previousTotal.expense);
    m_reportExpenseYoY = deltaText(total.expense, lastYearTotal.expense);
    m_reportIncomeMoM = deltaText(total.income, previousTotal.income);

    m_reportIncomeText = total.income.format();
    m_reportExpenseText = total.expense.format();
    m_reportBalanceText = total.balance().format();
    m_reportCountText = QString::number(total.count);
    m_reportTrend = trend;

    switch (months) {
    case 1:
        m_reportRangeLabel = QStringLiteral("%1 年 %2 月").arg(m_year).arg(m_month);
        break;
    case 12:
        m_reportRangeLabel = QStringLiteral("%1 年 1-%2 月").arg(m_year).arg(m_month);
        break;
    default:
        m_reportRangeLabel = QStringLiteral("近 %1 个月（%2-%3）")
                                 .arg(months)
                                 .arg(window.first().second)
                                 .arg(window.last().second);
        break;
    }

    // 分类占比（按累计金额降序）
    QVector<QPair<qint64, double>> ordered;
    ordered.reserve(shareAmounts.size());
    for (auto it = shareAmounts.cbegin(); it != shareAmounts.cend(); ++it)
        ordered.append(qMakePair(it.key(), it.value()));
    std::sort(ordered.begin(), ordered.end(),
              [](const QPair<qint64, double> &a, const QPair<qint64, double> &b) {
                  return a.second > b.second;
              });

    const double shareTotal = static_cast<double>(total.expense.cents());
    QVariantList shares;
    for (const auto &row : ordered) {
        const double ratio = shareTotal > 0 ? row.second / shareTotal : 0.0;
        QVariantMap item;
        item.insert(QStringLiteral("name"), shareNames.value(row.first));
        item.insert(QStringLiteral("color"), shareColors.value(row.first));
        item.insert(QStringLiteral("totalText"), Money(static_cast<qint64>(row.second)).format());
        item.insert(QStringLiteral("shareText"),
                    QStringLiteral("%1%").arg(QString::number(ratio * 100.0, 'f', 1)));
        item.insert(QStringLiteral("share"), ratio);
        shares.append(item);
    }
    m_reportShares = shares;

    QVector<AccountTotal> accounts;
    for (auto it = accountMap.cbegin(); it != accountMap.cend(); ++it)
        accounts.append(it.value());
    std::sort(accounts.begin(), accounts.end(), [](const AccountTotal &a, const AccountTotal &b) {
        return a.accountId < b.accountId;
    });

    QVariantList accountList;
    for (const AccountTotal &row : accounts) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), row.accountId);
        item.insert(QStringLiteral("name"), row.name);
        item.insert(QStringLiteral("color"), row.color);
        item.insert(QStringLiteral("incomeText"), row.income.format());
        item.insert(QStringLiteral("expenseText"), row.expense.format());
        item.insert(QStringLiteral("count"), row.count);
        accountList.append(item);
    }
    m_reportAccountTotals = accountList;

    // 标签维度：范围内按标签累计支出
    QHash<QString, double> tagAmounts;
    QHash<QString, int> tagCounts;
    for (const auto &entry : window) {
        for (const TagTotal &row : m_tagRepository.tagTotals(entry.first, entry.second)) {
            tagAmounts[row.name] += static_cast<double>(row.total.cents());
            tagCounts[row.name] += row.count;
        }
    }
    QVector<QPair<QString, double>> orderedTags;
    orderedTags.reserve(tagAmounts.size());
    for (auto it = tagAmounts.cbegin(); it != tagAmounts.cend(); ++it)
        orderedTags.append(qMakePair(it.key(), it.value()));
    std::sort(orderedTags.begin(), orderedTags.end(),
              [](const QPair<QString, double> &a, const QPair<QString, double> &b) {
                  return a.second > b.second;
              });

    QVariantList tagShares;
    for (const auto &row : orderedTags) {
        const double ratio = shareTotal > 0 ? row.second / shareTotal : 0.0;
        QVariantMap item;
        item.insert(QStringLiteral("name"), row.first);
        item.insert(QStringLiteral("totalText"), Money(static_cast<qint64>(row.second)).format());
        item.insert(QStringLiteral("shareText"),
                    QStringLiteral("%1%").arg(QString::number(ratio * 100.0, 'f', 1)));
        item.insert(QStringLiteral("share"), ratio);
        item.insert(QStringLiteral("count"), tagCounts.value(row.first));
        tagShares.append(item);
    }
    m_reportTagShares = tagShares;

    // 数据改动后（reload）报表也要通知 QML 刷新，否则绑定不会重新求值
    emit reportChanged();
}


bool LedgerViewModel::themeDark() const
{
    return m_settingsRepository.boolValue(QStringLiteral("theme.dark"), false);
}

void LedgerViewModel::saveThemeDark(bool dark)
{
    m_settingsRepository.setValue(QStringLiteral("theme.dark"),
                                  dark ? QStringLiteral("1") : QStringLiteral("0"));
}

QString LedgerViewModel::databasePath() const
{
    return m_db.databaseName();
}

void LedgerViewModel::openDataFolder()
{
    const QString path = m_db.databaseName();
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    QDesktopServices::openUrl(QUrl::fromLocalFile(info.absolutePath()));
}

int LedgerViewModel::totalTransactionCount() const
{
    return m_transactionRepository.countAll();
}

QString LedgerViewModel::documentsDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
}

QVariantMap LedgerViewModel::previewImportCsv(const QString &path)
{
    QVariantMap result;
    QString error;
    const ImportPreview preview = ImportService::preview(path, &error);

    result.insert(QStringLiteral("ok"), preview.ok);
    result.insert(QStringLiteral("message"), preview.message.isEmpty() ? error : preview.message);
    result.insert(QStringLiteral("source"), preview.source);
    result.insert(QStringLiteral("total"), preview.rows.size());
    result.insert(QStringLiteral("expenseCount"), preview.expenseCount());
    result.insert(QStringLiteral("incomeCount"), preview.incomeCount());
    result.insert(QStringLiteral("expenseText"), preview.expenseTotal().format());
    result.insert(QStringLiteral("incomeText"), preview.incomeTotal().format());

    QVariantList rows;
    const int limit = qMin(30, preview.rows.size());
    for (int i = 0; i < limit; ++i) {
        const ImportRow &row = preview.rows.at(i);
        QVariantMap item;
        item.insert(QStringLiteral("dateText"), row.occurredAt.toString(QStringLiteral("MM-dd HH:mm")));
        item.insert(QStringLiteral("kindText"), displayName(row.kind));
        item.insert(QStringLiteral("amountText"),
                    (row.kind == TransactionKind::Income ? QStringLiteral("+") : QStringLiteral("-"))
                        + row.amount.format());
        item.insert(QStringLiteral("note"), row.note);
        item.insert(QStringLiteral("payee"), row.payee);
        item.insert(QStringLiteral("externalId"), row.externalId);
        rows.append(item);
    }
    result.insert(QStringLiteral("rows"), rows);
    return result;
}

QVariantMap LedgerViewModel::importCsv(const QString &path, int accountId, int categoryId)
{
    QVariantMap result;
    QString error;
    const ImportPreview preview = ImportService::preview(path, &error);
    if (!preview.ok) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"),
                      preview.message.isEmpty() ? error : preview.message);
        return result;
    }

    const std::optional<qint64> category =
        categoryId > 0 ? std::optional<qint64>(categoryId) : std::nullopt;
    const ImportOutcome outcome =
        ImportService::import(m_transactionRepository, preview, accountId, category, &error);

    if (outcome.imported > 0)
        reload();

    result.insert(QStringLiteral("ok"), outcome.imported > 0);
    result.insert(QStringLiteral("imported"), outcome.imported);
    result.insert(QStringLiteral("skipped"), outcome.skipped);
    result.insert(QStringLiteral("message"),
                  QStringLiteral("导入 %1 笔，跳过 %2 笔（重复或无效）")
                      .arg(outcome.imported)
                      .arg(outcome.skipped));
    return result;
}


QVariantMap LedgerViewModel::exportReportPdf(const QString &directory)
{
    // 导出要写真实金额，临时关闭隐私掩码
    keep::Privacy::ScopedMaskOff maskGuard;

    QVariantMap result;

    QString targetDir = directory.trimmed();
    if (targetDir.isEmpty())
        targetDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (targetDir.isEmpty()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("找不到可写入的目录"));
        return result;
    }
    QDir().mkpath(targetDir);

    const QString fileName = QStringLiteral("摸鱼记账-报表-%1-%2-%3.pdf")
                                 .arg(m_year)
                                 .arg(m_month, 2, 10, QLatin1Char('0'))
                                 .arg(m_reportRange);
    const QString path = QDir(targetDir).filePath(fileName);

    QPdfWriter writer(path);
    writer.setPageSize(QPageSize(QPageSize::A4));
    writer.setResolution(150);
    writer.setTitle(QStringLiteral("摸鱼记账 报表"));

    QPainter painter(&writer);
    if (!painter.isActive()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("无法创建 PDF"));
        return result;
    }

    QFont titleFont(QStringLiteral("Microsoft YaHei"), 18, QFont::DemiBold);
    QFont sectionFont(QStringLiteral("Microsoft YaHei"), 11, QFont::DemiBold);
    QFont bodyFont(QStringLiteral("Microsoft YaHei"), 9);
    QFont smallFont(QStringLiteral("Microsoft YaHei"), 8);

    const int pageWidth = writer.width();
    const int margin = pageWidth / 14;
    int y = margin;

    painter.setPen(QColor(QStringLiteral("#111827")));
    painter.setFont(titleFont);
    painter.drawText(QRect(margin, y, pageWidth - margin * 2, margin),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("摸鱼记账 · 报表"));
    y += margin;

    painter.setFont(bodyFont);
    painter.setPen(QColor(QStringLiteral("#6B7280")));
    painter.drawText(QRect(margin, y, pageWidth - margin * 2, margin / 2),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("%1 · 生成于 %2")
                         .arg(m_reportRangeLabel,
                              QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm"))));
    y += margin;

    // KPI
    painter.setFont(sectionFont);
    painter.setPen(QColor(QStringLiteral("#111827")));
    painter.drawText(QRect(margin, y, pageWidth - margin * 2, margin / 2),
                     Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("总览"));
    y += margin / 2;

    const QStringList kpiLines{
        QStringLiteral("收入：%1    支出：%2    结余：%3    笔数：%4")
            .arg(m_reportIncomeText, m_reportExpenseText, m_reportBalanceText, m_reportCountText),
        QStringLiteral("支出环比：%1    支出同比：%2").arg(m_reportExpenseMoM, m_reportExpenseYoY),
    };
    painter.setFont(bodyFont);
    painter.setPen(QColor(QStringLiteral("#374151")));
    for (const QString &line : kpiLines) {
        painter.drawText(QRect(margin, y, pageWidth - margin * 2, margin / 3),
                         Qt::AlignLeft | Qt::AlignVCenter, line);
        y += margin / 3;
    }
    y += margin / 3;

    const auto drawTable = [&](const QString &title, const QStringList &headers,
                               const QVector<QStringList> &rows) {
        painter.setFont(sectionFont);
        painter.setPen(QColor(QStringLiteral("#111827")));
        painter.drawText(QRect(margin, y, pageWidth - margin * 2, margin / 2),
                         Qt::AlignLeft | Qt::AlignVCenter, title);
        y += margin / 2;

        const int tableWidth = pageWidth - margin * 2;
        const int columnWidth = headers.isEmpty() ? tableWidth : tableWidth / headers.size();
        painter.setFont(smallFont);
        painter.setPen(QColor(QStringLiteral("#6B7280")));
        for (int i = 0; i < headers.size(); ++i)
            painter.drawText(QRect(margin + columnWidth * i, y, columnWidth, margin / 3),
                             Qt::AlignLeft | Qt::AlignVCenter, headers.at(i));
        y += margin / 3;
        painter.setPen(QColor(QStringLiteral("#D1D5DB")));
        painter.drawLine(margin, y, pageWidth - margin, y);
        y += margin / 12;

        painter.setFont(bodyFont);
        for (const QStringList &row : rows) {
            painter.setPen(QColor(QStringLiteral("#111827")));
            for (int i = 0; i < row.size() && i < headers.size(); ++i) {
                const Qt::Alignment alignment =
                    i == 0 ? Qt::AlignLeft : Qt::AlignRight;
                painter.drawText(QRect(margin + columnWidth * i, y, columnWidth, margin / 3),
                                 alignment | Qt::AlignVCenter, row.at(i));
            }
            y += margin / 3;
        }
        y += margin / 4;
    };

    QVector<QStringList> shareRows;
    for (const QVariant &item : m_reportShares) {
        const QVariantMap row = item.toMap();
        shareRows.append({row.value(QStringLiteral("name")).toString(),
                          row.value(QStringLiteral("totalText")).toString(),
                          row.value(QStringLiteral("shareText")).toString()});
    }
    drawTable(QStringLiteral("支出分类排行"),
              {QStringLiteral("分类"), QStringLiteral("金额"), QStringLiteral("占比")}, shareRows);

    QVector<QStringList> accountRows;
    for (const QVariant &item : m_reportAccountTotals) {
        const QVariantMap row = item.toMap();
        accountRows.append({row.value(QStringLiteral("name")).toString(),
                            row.value(QStringLiteral("incomeText")).toString(),
                            row.value(QStringLiteral("expenseText")).toString(),
                            row.value(QStringLiteral("count")).toString()});
    }
    drawTable(QStringLiteral("账户维度"),
              {QStringLiteral("账户"), QStringLiteral("收入"), QStringLiteral("支出"),
               QStringLiteral("笔数")},
              accountRows);

    QVector<QStringList> tagRows;
    for (const QVariant &item : m_reportTagShares) {
        const QVariantMap row = item.toMap();
        tagRows.append({row.value(QStringLiteral("name")).toString(),
                        row.value(QStringLiteral("totalText")).toString(),
                        row.value(QStringLiteral("shareText")).toString()});
    }
    drawTable(QStringLiteral("标签维度"),
              {QStringLiteral("标签"), QStringLiteral("支出"), QStringLiteral("占比")}, tagRows);

    QVector<QStringList> monthRows;
    for (const QVariant &item : m_reportMonthlyRows) {
        const QVariantMap row = item.toMap();
        monthRows.append({row.value(QStringLiteral("label")).toString(),
                          row.value(QStringLiteral("incomeText")).toString(),
                          row.value(QStringLiteral("expenseText")).toString(),
                          row.value(QStringLiteral("count")).toString()});
    }
    drawTable(QStringLiteral("月度明细"),
              {QStringLiteral("月份"), QStringLiteral("收入"), QStringLiteral("支出"),
               QStringLiteral("笔数")},
              monthRows);

    painter.setFont(smallFont);
    painter.setPen(QColor(QStringLiteral("#9CA3AF")));
    painter.drawText(QRect(margin, writer.height() - margin / 2, pageWidth - margin * 2, margin / 2),
                     Qt::AlignLeft | Qt::AlignVCenter,
                     QStringLiteral("摸鱼记账 v%1 · 本地单机记账")
                         .arg(QCoreApplication::applicationVersion()));
    painter.end();

    const QFileInfo info(path);
    if (!info.exists() || info.size() < 512) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("PDF 生成失败"));
        return result;
    }

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"), QStringLiteral("报表已导出到 %1").arg(path));
    result.insert(QStringLiteral("path"), path);
    return result;
}

QVariantMap LedgerViewModel::exportMonthCsv(const QString &directory)
{
    // 导出要写真实金额，临时关闭隐私掩码
    keep::Privacy::ScopedMaskOff maskGuard;

    QVariantMap result;

    QString targetDir = directory.trimmed();
    if (targetDir.isEmpty())
        targetDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    if (targetDir.isEmpty()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("找不到可写入的目录"));
        return result;
    }
    QDir().mkpath(targetDir);

    const QString fileName =
        QStringLiteral("摸鱼记账-%1-%2.csv").arg(m_year).arg(m_month, 2, 10, QLatin1Char('0'));
    const QString path = QDir(targetDir).filePath(fileName);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("无法写入文件：%1").arg(file.errorString()));
        return result;
    }

    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    stream.setGenerateByteOrderMark(true);  // Excel 打开中文不乱码

    const QVector<Category> categories = m_categoryRepository.listAll(true);
    QHash<qint64, QString> categoryNames;
    for (const Category &category : categories)
        categoryNames.insert(category.id, category.name);

    QHash<qint64, QString> accountNames;
    for (const AccountWithBalance &row : m_accountRepository.listWithBalances(true))
        accountNames.insert(row.account.id, row.account.name);

    stream << "日期,类型,分类,账户,金额,备注\n";
    for (const Transaction &row : m_transactionRepository.listByMonth(m_year, m_month)) {
        stream << row.occurredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm")) << ','
               << displayName(row.kind) << ','
               << csvField(categoryNames.value(row.categoryId.value_or(0))) << ','
               << csvField(accountNames.value(row.accountId)) << ','
               << (row.kind == TransactionKind::Income ? QStringLiteral("+") : QStringLiteral("-"))
                      + row.amount.format(false)
               << ',' << csvField(row.note) << '\n';
    }
    stream.flush();
    file.close();

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"), QStringLiteral("已导出到 %1").arg(path));
    result.insert(QStringLiteral("path"), path);
    return result;
}

QVariantMap LedgerViewModel::addTransaction(const QString &kind, const QString &amountText,
                                            int accountId, int categoryId,
                                            const QString &dateText, const QString &note,
                                            const QString &tags)
{
    QVariantMap result;
    const auto amount = Money::fromYuan(amountText);
    if (!amount.has_value() || amount->cents() <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("金额格式不正确"));
        return result;
    }
    if (accountId <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("请选择付款账户"));
        return result;
    }
    if (categoryId <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("请选择分类"));
        return result;
    }

    const QDate date = QDate::fromString(dateText.trimmed(), QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("日期格式应为 2026-11-11"));
        return result;
    }

    keep::Transaction transaction;
    transaction.kind = (kind == QLatin1String("income")) ? TransactionKind::Income
                                                          : TransactionKind::Expense;
    transaction.amount = *amount;
    transaction.accountId = accountId;
    transaction.categoryId = categoryId;
    transaction.occurredAt = QDateTime(date, QTime::currentTime());
    transaction.note = note.trimmed();

    const QStringList tagNames = splitTagText(tags);

    QString error;
    const auto id = m_transactionRepository.insert(transaction, &error);
    if (!id.has_value()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), error.isEmpty() ? QStringLiteral("写入失败") : error);
        return result;
    }

    if (!tagNames.isEmpty())
        m_tagRepository.setTags(*id, tagNames);

    reload();

    // 压入撤销栈（动作已执行，命令只负责 undo/redo 的后续执行）
    auto inserted = std::make_shared<keep::Transaction>(transaction);
    inserted->id = *id;
    auto insertedId = std::make_shared<qint64>(*id);
    auto insertedTags = std::make_shared<QStringList>(tagNames);
    m_undoStack.push(new FunctionCommand(
        [this, inserted, insertedId, insertedTags] {
            const auto newId = m_transactionRepository.insert(*inserted);
            if (newId.has_value()) {
                *insertedId = *newId;
                inserted->id = *newId;
                if (!insertedTags->isEmpty())
                    m_tagRepository.setTags(*newId, *insertedTags);
            }
            reload();
        },
        [this, insertedId] {
            if (*insertedId > 0)
                m_transactionRepository.remove(*insertedId);
            reload();
        },
        QStringLiteral("新增一笔")));

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"), QStringLiteral("已保存，可 Ctrl+Z 撤销"));
    result.insert(QStringLiteral("id"), *id);
    return result;
}

namespace {

QVariantMap accountResult(bool ok, const QString &message)
{
    QVariantMap result;
    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("message"), message);
    return result;
}

const char *const kAccountColors[] = {"#3B82F6", "#06B6D4", "#22C55E", "#F59E0B",
                                      "#64748B", "#8B5CF6", "#EF4444", "#14B8A6"};

}  // namespace

QVariantMap LedgerViewModel::addAccount(const QString &name, const QString &type,
                                        const QString &openingBalanceText,
                                        const QString &creditLimitText)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return accountResult(false, QStringLiteral("账户名不能为空"));

    const auto accountType = accountTypeFromDb(type);
    if (!accountType.has_value())
        return accountResult(false, QStringLiteral("账户类型无效"));

    Money opening;
    if (!openingBalanceText.trimmed().isEmpty()) {
        const auto parsed = Money::fromYuan(openingBalanceText);
        if (!parsed.has_value())
            return accountResult(false, QStringLiteral("期初余额格式不正确"));
        opening = *parsed;
    }

    std::optional<qint64> creditLimit;
    if (!creditLimitText.trimmed().isEmpty()) {
        const auto parsed = Money::fromYuan(creditLimitText);
        if (!parsed.has_value())
            return accountResult(false, QStringLiteral("额度格式不正确"));
        creditLimit = parsed->cents();
    }

    keep::Account account;
    // 新账户归入当前账本
    account.ledgerId = m_currentLedgerId > 0
                           ? std::optional<qint64>(m_currentLedgerId)
                           : std::nullopt;
    account.name = trimmed;
    account.type = *accountType;
    account.openingBalance = opening;
    account.creditLimitCents = creditLimit;
    account.icon = trimmed.left(1);
    account.color = QString::fromLatin1(kAccountColors[m_accountModel.count()
                                                       % (sizeof(kAccountColors)
                                                          / sizeof(kAccountColors[0]))]);
    account.sortOrder = m_accountModel.count();

    QString error;
    const auto id = m_accountRepository.insert(account, &error);
    if (!id.has_value()) {
        // 账户名在「同一账本内」唯一：跨账本允许重名，同账本重名给出友好提示
        if (error.contains(QStringLiteral("UNIQUE constraint failed"), Qt::CaseInsensitive)) {
            return accountResult(false, QStringLiteral("该账本下已存在同名账户，请换一个名称"));
        }
        return accountResult(false, error.isEmpty() ? QStringLiteral("新增账户失败") : error);
    }

    reload();
    QVariantMap result = accountResult(true, QStringLiteral("账户已新增"));
    result.insert(QStringLiteral("id"), *id);
    return result;
}

QVariantMap LedgerViewModel::updateAccount(qint64 id, const QString &name, const QString &type,
                                           const QString &openingBalanceText,
                                           const QString &creditLimitText)
{
    const auto existing = m_accountRepository.findById(id);
    if (!existing.has_value())
        return accountResult(false, QStringLiteral("账户不存在"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return accountResult(false, QStringLiteral("账户名不能为空"));

    const auto accountType = accountTypeFromDb(type);
    if (!accountType.has_value())
        return accountResult(false, QStringLiteral("账户类型无效"));

    Money opening;
    if (!openingBalanceText.trimmed().isEmpty()) {
        const auto parsed = Money::fromYuan(openingBalanceText);
        if (!parsed.has_value())
            return accountResult(false, QStringLiteral("期初余额格式不正确"));
        opening = *parsed;
    }

    std::optional<qint64> creditLimit;
    if (!creditLimitText.trimmed().isEmpty()) {
        const auto parsed = Money::fromYuan(creditLimitText);
        if (!parsed.has_value())
            return accountResult(false, QStringLiteral("额度格式不正确"));
        creditLimit = parsed->cents();
    }

    keep::Account account = *existing;
    account.name = trimmed;
    account.type = *accountType;
    account.openingBalance = opening;
    account.creditLimitCents = creditLimit;

    QString error;
    if (!m_accountRepository.update(account, &error))
        return accountResult(false, error.isEmpty() ? QStringLiteral("更新账户失败") : error);

    reload();
    return accountResult(true, QStringLiteral("账户已更新"));
}

QVariantMap LedgerViewModel::archiveAccount(qint64 id, bool archived)
{
    QString error;
    if (!m_accountRepository.setArchived(id, archived, &error))
        return accountResult(false, error.isEmpty() ? QStringLiteral("操作失败") : error);

    reload();
    return accountResult(true, archived ? QStringLiteral("账户已归档")
                                        : QStringLiteral("账户已恢复"));
}

QVariantMap LedgerViewModel::accountDetail(qint64 id)
{
    QVariantMap map;
    const auto account = m_accountRepository.findById(id);
    if (!account.has_value())
        return map;

    const auto balance = m_accountRepository.balance(id);
    map.insert(QStringLiteral("id"), account->id);
    map.insert(QStringLiteral("name"), account->name);
    map.insert(QStringLiteral("type"), toDbString(account->type));
    map.insert(QStringLiteral("typeText"), displayName(account->type));
    map.insert(QStringLiteral("openingText"), account->openingBalance.format(false));
    map.insert(QStringLiteral("creditLimitText"),
               account->creditLimitCents ? Money(*account->creditLimitCents).format(false)
                                         : QString());
    map.insert(QStringLiteral("balanceText"), balance ? balance->format() : QStringLiteral("—"));
    map.insert(QStringLiteral("archived"), account->archived);
    return map;
}

QVariantMap LedgerViewModel::updateTransaction(qint64 id, const QString &kind, const QString &amountText,
                                               int accountId, int categoryId, const QString &dateText,
                                               const QString &note, const QString &tags)
{
    QVariantMap result;
    const auto before = m_transactionRepository.findById(id);
    if (!before.has_value()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("这笔流水不存在"));
        return result;
    }

    const auto amount = Money::fromYuan(amountText);
    if (!amount.has_value() || amount->cents() <= 0) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("金额格式不正确"));
        return result;
    }
    const QDate date = QDate::fromString(dateText.trimmed(), QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("日期格式应为 2026-11-11"));
        return result;
    }

    keep::Transaction after = *before;
    after.kind = (kind == QLatin1String("income")) ? TransactionKind::Income : TransactionKind::Expense;
    after.amount = *amount;
    after.accountId = accountId;
    after.categoryId = categoryId;
    after.occurredAt = QDateTime(date, before->occurredAt.time());
    after.note = note.trimmed();

    QString error;
    if (!m_transactionRepository.update(after, &error)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"),
                      error.isEmpty() ? QStringLiteral("更新失败") : error);
        return result;
    }

    // 标签是覆盖式写入：空字符串表示清空
    const QStringList tagNames = splitTagText(tags);
    const QStringList previousTags = m_tagRepository.tagsFor(id);
    m_tagRepository.setTags(id, tagNames);

    reload();

    auto beforeCopy = std::make_shared<keep::Transaction>(*before);
    auto afterCopy = std::make_shared<keep::Transaction>(after);
    auto beforeTags = std::make_shared<QStringList>(previousTags);
    auto afterTags = std::make_shared<QStringList>(tagNames);
    m_undoStack.push(new FunctionCommand(
        [this, afterCopy, afterTags] {
            m_transactionRepository.update(*afterCopy);
            m_tagRepository.setTags(afterCopy->id, *afterTags);
            reload();
        },
        [this, beforeCopy, beforeTags] {
            m_transactionRepository.update(*beforeCopy);
            m_tagRepository.setTags(beforeCopy->id, *beforeTags);
            reload();
        },
        QStringLiteral("编辑一笔")));

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"), QStringLiteral("已更新，可 Ctrl+Z 撤销"));
    result.insert(QStringLiteral("id"), id);
    return result;
}


namespace {

keep::RecurringRule buildRule(qint64 id, const QString &kind, const QString &amountText,
                              int accountId, int categoryId, const QString &note, int dayOfMonth)
{
    keep::RecurringRule rule;
    rule.id = id;
    rule.kind = (kind == QLatin1String("income")) ? keep::TransactionKind::Income
                                                  : keep::TransactionKind::Expense;
    if (const auto amount = keep::Money::fromYuan(amountText); amount.has_value())
        rule.amount = *amount;
    rule.accountId = accountId;
    if (categoryId > 0)
        rule.categoryId = categoryId;
    rule.note = note.trimmed();
    rule.dayOfMonth = qBound(1, dayOfMonth, 28);
    rule.enabled = true;
    return rule;
}

}  // namespace


void LedgerViewModel::applyLedgerScope()
{
    const std::optional<qint64> scope =
        m_currentLedgerId > 0 ? std::optional<qint64>(m_currentLedgerId) : std::nullopt;
    m_transactionRepository.setLedgerScope(scope);
    m_accountRepository.setLedgerScope(scope);
    m_tagRepository.setLedgerScope(scope);
}

QVariantMap LedgerViewModel::switchLedger(int ledgerId)
{
    if (ledgerId <= 0)
        return budgetResult(false, QStringLiteral("账本无效"));
    if (ledgerId == m_currentLedgerId)
        return budgetResult(true, QStringLiteral("已在该账本"));

    bool exists = false;
    for (const Ledger &ledger : m_ledgerRepository.listAll()) {
        if (ledger.id == ledgerId) {
            exists = true;
            m_currentLedgerName = ledger.name;
        }
    }
    if (!exists)
        return budgetResult(false, QStringLiteral("账本不存在"));

    m_currentLedgerId = ledgerId;
    m_settingsRepository.setValue(QStringLiteral("ledger.current"),
                                  QString::number(ledgerId));
    applyLedgerScope();
    reload();
    return budgetResult(true, QStringLiteral("已切换到「%1」").arg(m_currentLedgerName));
}

QVariantMap LedgerViewModel::addLedger(const QString &name)
{
    QString error;
    const auto id = m_ledgerRepository.insert(name, QString(), &error);
    if (!id.has_value())
        return budgetResult(false, error.isEmpty() ? QStringLiteral("创建失败") : error);

    reload();
    switchLedger(static_cast<int>(*id));
    return budgetResult(true, QStringLiteral("已创建账本「%1」").arg(name.trimmed()));
}

QVariantMap LedgerViewModel::renameLedger(int ledgerId, const QString &name)
{
    QString error;
    if (!m_ledgerRepository.rename(ledgerId, name, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("重命名失败") : error);
    reload();
    return budgetResult(true, QStringLiteral("已重命名"));
}

QVariantMap LedgerViewModel::removeLedger(int ledgerId)
{
    QString error;
    if (!m_ledgerRepository.remove(ledgerId, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("删除失败") : error);
    if (ledgerId == m_currentLedgerId) {
        if (const auto defaultLedger = m_ledgerRepository.defaultId(); defaultLedger.has_value()) {
            m_currentLedgerId = *defaultLedger;
            applyLedgerScope();
        }
    }
    reload();
    return budgetResult(true, QStringLiteral("已删除账本"));
}

QVariantMap LedgerViewModel::addRecurring(const QString &kind, const QString &amountText,
                                          int accountId, int categoryId, const QString &note,
                                          int dayOfMonth)
{
    const auto amount = Money::fromYuan(amountText);
    if (!amount.has_value() || amount->cents() <= 0)
        return budgetResult(false, QStringLiteral("金额格式不正确"));
    if (accountId <= 0)
        return budgetResult(false, QStringLiteral("请选择账户"));

    RecurringRule rule = buildRule(0, kind, amountText, accountId, categoryId, note, dayOfMonth);
    QString error;
    if (!m_recurringRepository.insert(rule, &error).has_value())
        return budgetResult(false, error.isEmpty() ? QStringLiteral("保存失败") : error);

    reload();
    return budgetResult(true, QStringLiteral("已添加定期账单"));
}

QVariantMap LedgerViewModel::updateRecurring(qint64 id, const QString &kind,
                                             const QString &amountText, int accountId,
                                             int categoryId, const QString &note, int dayOfMonth)
{
    const auto amount = Money::fromYuan(amountText);
    if (!amount.has_value() || amount->cents() <= 0)
        return budgetResult(false, QStringLiteral("金额格式不正确"));

    RecurringRule rule = buildRule(id, kind, amountText, accountId, categoryId, note, dayOfMonth);
    QString error;
    if (!m_recurringRepository.update(rule, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("更新失败") : error);

    reload();
    return budgetResult(true, QStringLiteral("已更新定期账单"));
}

QVariantMap LedgerViewModel::removeRecurring(qint64 id)
{
    QString error;
    if (!m_recurringRepository.remove(id, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("删除失败") : error);
    reload();
    return budgetResult(true, QStringLiteral("已删除定期账单"));
}

QVariantMap LedgerViewModel::setRecurringEnabled(qint64 id, bool enabled)
{
    QString error;
    if (!m_recurringRepository.setEnabled(id, enabled, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("更新失败") : error);
    reload();
    return budgetResult(true, enabled ? QStringLiteral("已启用") : QStringLiteral("已暂停"));
}

QVariantMap LedgerViewModel::generateRecurring()
{
    QString error;
    const int generated =
        m_recurringRepository.generateForMonth(m_transactionRepository, m_year, m_month, &error);
    if (generated > 0)
        reload();
    return budgetResult(generated > 0,
                        generated > 0
                            ? QStringLiteral("已自动生成 %1 笔定期账单").arg(generated)
                            : (error.isEmpty() ? QStringLiteral("本月没有待生成的定期账单") : error));
}


QVariantMap LedgerViewModel::backupNow()
{
    QString path;
    QString error;
    if (!BackupService::backupNow(m_db, m_db.databaseName(), &path, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("备份失败") : error);

    reload();
    return budgetResult(true, QStringLiteral("已备份到 %1").arg(QFileInfo(path).fileName()));
}

QJsonObject LedgerViewModel::buildFullExport() const
{
    QJsonObject root;
    root.insert(QStringLiteral("format"), QStringLiteral("moyu_ledger.full_export"));
    root.insert(QStringLiteral("formatVersion"), 1);
    root.insert(QStringLiteral("appVersion"), QCoreApplication::applicationVersion());
    root.insert(QStringLiteral("exportedAt"),
                QDateTime::currentDateTime().toString(Qt::ISODate));

    int schemaVersion = 0;
    {
        QSqlQuery query(m_db);
        if (query.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) && query.next())
            schemaVersion = query.value(0).toInt();
    }
    root.insert(QStringLiteral("schemaVersion"), schemaVersion);

    QJsonObject data;
    const QStringList tables{
        QStringLiteral("ledgers"),      QStringLiteral("accounts"),
        QStringLiteral("categories"),   QStringLiteral("transactions"),
        QStringLiteral("tags"),         QStringLiteral("transaction_tags"),
        QStringLiteral("budgets"),      QStringLiteral("recurring_rules"),
        QStringLiteral("settings"),
    };

    for (const QString &table : tables) {
        QJsonArray rows;
        QSqlQuery query(m_db);
        if (!query.exec(QStringLiteral("SELECT * FROM %1").arg(table)))
            continue;

        const QSqlRecord record = query.record();
        while (query.next()) {
            QJsonObject row;
            for (int i = 0; i < record.count(); ++i) {
                const QVariant value = query.value(i);
                const QString name = record.fieldName(i);
                if (value.isNull()) {
                    row.insert(name, QJsonValue());
                } else {
                    const int type = value.typeId();
                    if (type == QMetaType::LongLong || type == QMetaType::Int
                        || type == QMetaType::Short) {
                        row.insert(name, static_cast<double>(value.toLongLong()));
                    } else if (type == QMetaType::Double) {
                        row.insert(name, value.toDouble());
                    } else {
                        row.insert(name, value.toString());
                    }
                }
            }
            rows.append(row);
        }
        data.insert(table, rows);
    }
    root.insert(QStringLiteral("data"), data);
    return root;
}

QVariantMap LedgerViewModel::exportAllJson(const QString &directory)
{
    // 导出要写真实金额，临时关闭隐私掩码
    keep::Privacy::ScopedMaskOff maskGuard;

    QString targetDir = directory.trimmed();
    if (targetDir.isEmpty())
        targetDir = documentsDirectory();
    if (targetDir.isEmpty())
        return budgetResult(false, QStringLiteral("找不到可写入的目录"));
    QDir().mkpath(targetDir);

    const QString fileName = QStringLiteral("摸鱼记账-全量导出-%1.json")
                                 .arg(QDateTime::currentDateTime().toString(
                                     QStringLiteral("yyyyMMdd-HHmmss")));
    const QString path = QDir(targetDir).filePath(fileName);

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return budgetResult(false, QStringLiteral("无法写入文件：%1").arg(file.errorString()));
    file.write(QJsonDocument(buildFullExport()).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return budgetResult(false, QStringLiteral("写入失败：%1").arg(file.errorString()));

    QVariantMap result = budgetResult(true, QStringLiteral("已导出全量数据到 %1").arg(path));
    result.insert(QStringLiteral("path"), path);
    result.insert(QStringLiteral("size"), QFileInfo(path).size());
    return result;
}

QVariantMap LedgerViewModel::exportBackup(const QString &filePath)
{
    keep::Privacy::ScopedMaskOff maskGuard;

    QString target = filePath.trimmed();
    if (target.isEmpty())
        return budgetResult(false, QStringLiteral("请选择保存位置"));
    if (!target.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive))
        target += QStringLiteral(".json");

    const QString directory = QFileInfo(target).absolutePath();
    if (directory.isEmpty() || !QDir().mkpath(directory))
        return budgetResult(false, QStringLiteral("无法创建目录：%1").arg(directory));

    QSaveFile file(target);
    if (!file.open(QIODevice::WriteOnly))
        return budgetResult(false, QStringLiteral("无法写入文件：%1").arg(file.errorString()));
    file.write(QJsonDocument(buildFullExport()).toJson(QJsonDocument::Indented));
    if (!file.commit())
        return budgetResult(false, QStringLiteral("写入失败：%1").arg(file.errorString()));

    QVariantMap result = budgetResult(true, QStringLiteral("已导出到 %1").arg(target));
    result.insert(QStringLiteral("path"), target);
    result.insert(QStringLiteral("size"), QFileInfo(target).size());
    return result;
}

QVariantMap LedgerViewModel::importBackup(const QString &filePath)
{
    keep::Privacy::ScopedMaskOff maskGuard;

    QFile file(filePath.trimmed());
    if (!file.open(QIODevice::ReadOnly))
        return budgetResult(false, QStringLiteral("无法读取文件：%1").arg(file.errorString()));

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    file.close();
    if (parseError.error != QJsonParseError::NoError || !document.isObject())
        return budgetResult(false, QStringLiteral("JSON 解析失败：%1").arg(parseError.errorString()));

    const QJsonObject root = document.object();
    // 兼容改名前的旧备份（keep_account.full_export）
    const QString formatId = root.value(QStringLiteral("format")).toString();
    if (formatId != QLatin1String("moyu_ledger.full_export")
        && formatId != QLatin1String("keep_account.full_export")) {
        return budgetResult(false, QStringLiteral("这不是「摸鱼记账」导出的全量备份文件"));
    }
    const QJsonObject data = root.value(QStringLiteral("data")).toObject();
    if (data.isEmpty())
        return budgetResult(false, QStringLiteral("备份文件里没有数据"));

    // 导入前先备份当前数据库，误操作可回滚
    QString backupPath;
    QString backupError;
    if (!BackupService::backupNow(m_db, m_db.databaseName(), &backupPath, &backupError))
        return budgetResult(false, QStringLiteral("导入前自动备份失败：%1").arg(backupError));

    const QStringList deleteOrder{
        QStringLiteral("transaction_tags"), QStringLiteral("tags"),
        QStringLiteral("transactions"),     QStringLiteral("budgets"),
        QStringLiteral("recurring_rules"),  QStringLiteral("accounts"),
        QStringLiteral("categories"),       QStringLiteral("ledgers"),
        QStringLiteral("settings"),
    };
    const QStringList insertOrder{
        QStringLiteral("ledgers"),      QStringLiteral("categories"),
        QStringLiteral("accounts"),     QStringLiteral("transactions"),
        QStringLiteral("tags"),         QStringLiteral("transaction_tags"),
        QStringLiteral("budgets"),      QStringLiteral("recurring_rules"),
        QStringLiteral("settings"),
    };

    auto fail = [this](const QString &message) {
        m_db.rollback();
        QSqlQuery(m_db).exec(QStringLiteral("PRAGMA foreign_keys = ON"));
        return budgetResult(false, message);
    };

    m_db.exec(QStringLiteral("PRAGMA foreign_keys = OFF"));
    if (!m_db.transaction())
        return fail(QStringLiteral("无法开始事务"));

    for (const QString &table : deleteOrder) {
        QSqlQuery query(m_db);
        if (!query.exec(QStringLiteral("DELETE FROM %1").arg(table)))
            return fail(QStringLiteral("清空 %1 失败：%2").arg(table, query.lastError().text()));
    }

    int totalRows = 0;
    for (const QString &table : insertOrder) {
        const QJsonArray rows = data.value(table).toArray();
        for (const QJsonValue &value : rows) {
            const QJsonObject row = value.toObject();
            if (row.isEmpty())
                continue;
            QStringList columns;
            QVariantList values;
            for (auto it = row.constBegin(); it != row.constEnd(); ++it) {
                columns << it.key();
                const QJsonValue field = it.value();
                if (field.isNull() || field.isUndefined()) {
                    values << QVariant();
                } else if (field.isBool()) {
                    values << (field.toBool() ? 1 : 0);
                } else if (field.isDouble()) {
                    const double number = field.toDouble();
                    if (std::floor(number) == number && std::abs(number) < 9.0e15)
                        values << static_cast<qint64>(number);
                    else
                        values << number;
                } else {
                    values << field.toVariant();
                }
            }

            QStringList placeholders;
            for (int i = 0; i < columns.size(); ++i)
                placeholders << QStringLiteral("?");
            const QString sql = QStringLiteral("INSERT INTO %1(%2) VALUES(%3)")
                                    .arg(table, columns.join(QStringLiteral(",")),
                                         placeholders.join(QStringLiteral(",")));
            QSqlQuery query(m_db);
            if (!query.prepare(sql))
                return fail(QStringLiteral("准备写入 %1 失败：%2").arg(table, query.lastError().text()));
            for (const QVariant &value : values)
                query.addBindValue(value);
            if (!query.exec())
                return fail(QStringLiteral("写入 %1 失败：%2").arg(table, query.lastError().text()));
            ++totalRows;
        }
    }

    if (!m_db.commit())
        return fail(QStringLiteral("提交失败：%1").arg(m_db.lastError().text()));
    m_db.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

    // 当前账本可能在导入后不存在了 → 回落到第一个账本
    const QVector<Ledger> ledgers = m_ledgerRepository.listAll();
    bool currentExists = false;
    for (const Ledger &ledger : ledgers) {
        if (ledger.id == m_currentLedgerId) {
            currentExists = true;
            break;
        }
    }
    if (!currentExists) {
        m_currentLedgerId = ledgers.isEmpty() ? 0 : ledgers.first().id;
        applyLedgerScope();
    }

    reload();

    QVariantMap result = budgetResult(
        true, QStringLiteral("已导入 %1 行数据（账户 %2 / 流水 %3）；导入前已备份为 %4")
                  .arg(QString::number(totalRows),
                       QString::number(m_accountRepository.listAll(true).size()),
                       QString::number(m_transactionRepository.countAll()),
                       QFileInfo(backupPath).fileName()));
    result.insert(QStringLiteral("accounts"), m_accountRepository.listAll(true).size());
    result.insert(QStringLiteral("transactions"), m_transactionRepository.countAll());
    result.insert(QStringLiteral("backupPath"), backupPath);
    return result;
}
QVariantMap LedgerViewModel::addCategory(const QString &kind, const QString &name,
                                         const QString &color, int parentId)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return budgetResult(false, QStringLiteral("分类名称不能为空"));

    CategoryKind categoryKind =
        kind == QLatin1String("income") ? CategoryKind::Income : CategoryKind::Expense;
    std::optional<qint64> parent;
    if (parentId > 0) {
        const auto parentCategory = m_categoryRepository.findById(parentId);
        if (!parentCategory.has_value())
            return budgetResult(false, QStringLiteral("父分类不存在"));
        if (parentCategory->parentId.has_value())
            return budgetResult(false, QStringLiteral("不支持三级分类"));
        parent = parentId;
        categoryKind = parentCategory->kind;
    }

    QString error;
    if (!parent.has_value()
        && m_categoryRepository.findByName(trimmed, categoryKind, &error).has_value()) {
        return budgetResult(false, QStringLiteral("该分类已存在"));
    }

    Category category;
    category.parentId = parent;
    category.kind = categoryKind;
    category.name = trimmed;
    category.icon = trimmed.left(1);
    category.color = color.trimmed();
    if (category.color.isEmpty() && parent.has_value()) {
        if (const auto parentCategory = m_categoryRepository.findById(*parent);
            parentCategory.has_value() && !parentCategory->color.isEmpty()) {
            category.color = parentCategory->color;
        }
    }
    if (category.color.isEmpty())
        category.color = QStringLiteral("#64748B");
    category.sortOrder = 100 + m_categoryRepository.count();

    const auto inserted = m_categoryRepository.insert(category, &error);
    if (!inserted.has_value()) {
        const bool duplicate = error.contains(QStringLiteral("UNIQUE"), Qt::CaseInsensitive);
        return budgetResult(false, duplicate ? QStringLiteral("同一分类下已有同名子类")
                                             : (error.isEmpty() ? QStringLiteral("保存失败") : error));
    }

    reload();
    QVariantMap result = budgetResult(true,
                                      parent.has_value()
                                          ? QStringLiteral("已新增小类「%1」").arg(trimmed)
                                          : QStringLiteral("已新增分类「%1」").arg(trimmed));
    result.insert(QStringLiteral("id"), *inserted);
    return result;
}

QVariantMap LedgerViewModel::updateCategory(int categoryId, const QString &name,
                                            const QString &color)
{
    const auto existing = m_categoryRepository.findById(categoryId);
    if (!existing.has_value())
        return budgetResult(false, QStringLiteral("分类不存在"));

    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty())
        return budgetResult(false, QStringLiteral("分类名称不能为空"));

    Category category = *existing;
    category.name = trimmed;
    if (!color.trimmed().isEmpty())
        category.color = color.trimmed();

    QString error;
    if (!m_categoryRepository.update(category, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("更新失败") : error);

    reload();
    return budgetResult(true, QStringLiteral("分类已更新"));
}

QVariantMap LedgerViewModel::archiveCategory(int categoryId, bool archived)
{
    QString error;
    if (!m_categoryRepository.setArchived(categoryId, archived, &error))
        return budgetResult(false, error.isEmpty() ? QStringLiteral("操作失败") : error);

    reload();
    return budgetResult(true, archived ? QStringLiteral("分类已归档") : QStringLiteral("分类已恢复"));
}


namespace {

// 解析 "yyyy-MM-dd" + "HH:mm"（timeText 为空则取当前时间）
QDateTime parseEntryMoment(const QString &dateText, const QString &timeText)
{
    const QDate date = QDate::fromString(dateText.trimmed(), QStringLiteral("yyyy-MM-dd"));
    const QString time = timeText.trimmed();
    QTime moment = QTime::currentTime();
    if (!time.isEmpty()) {
        const QTime parsed = QTime::fromString(time, QStringLiteral("HH:mm"));
        if (!parsed.isValid()) {
            const QTime parsedHm = QTime::fromString(time, QStringLiteral("H:mm"));
            if (parsedHm.isValid())
                moment = parsedHm;
        } else {
            moment = parsed;
        }
    }
    moment.setHMS(moment.hour(), moment.minute(), 0);  // 秒固定 00
    if (!date.isValid())
        return QDateTime::currentDateTime();
    return QDateTime(date, moment);
}

}  // namespace


QVariantMap LedgerViewModel::categoryBudgetSummary(int categoryId)
{
    QVariantMap result;
    if (categoryId <= 0)
        return result;

    // 该大类本身 + 其所有小类的 id 集合
    QSet<qint64> ids;
    ids.insert(categoryId);
    for (const Category &category : m_categoryRepository.listAll(true)) {
        if (category.parentId.has_value() && *category.parentId == categoryId)
            ids.insert(category.id);
    }

    QString name;
    for (const Category &category : m_categoryRepository.listAll(true)) {
        if (category.id == categoryId) {
            name = category.name;
            break;
        }
    }

    Money spent;
    for (const CategoryTotal &total : m_transactionRepository.categoryShares(m_year, m_month)) {
        if (ids.contains(total.categoryId))
            spent += total.total;
    }

    Money budget;
    const QString monthKey = QStringLiteral("%1-%2")
                                 .arg(m_year)
                                 .arg(m_month, 2, 10, QLatin1Char('0'));
    for (const BudgetRepository::CategoryBudget &row : m_budgetRepository.categoryBudgets(monthKey)) {
        if (row.categoryId == categoryId)
            budget = row.amount;
    }

    const bool hasBudget = budget.cents() > 0;
    const double ratio = hasBudget ? static_cast<double>(spent.cents())
                                         / static_cast<double>(budget.cents())
                                   : 0.0;

    result.insert(QStringLiteral("name"), name);
    result.insert(QStringLiteral("spentText"), spent.format());
    result.insert(QStringLiteral("hasBudget"), hasBudget);
    result.insert(QStringLiteral("budgetText"), hasBudget ? budget.format() : QString());
    result.insert(QStringLiteral("remainText"),
                  hasBudget ? Money(budget.cents() - spent.cents()).format() : QString());
    result.insert(QStringLiteral("ratio"), ratio);
    result.insert(QStringLiteral("overLimit"), hasBudget && spent.cents() > budget.cents());
    return result;
}


namespace {

QString investmentDateText(const QDate &date)
{
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

// 投资类账户每日记录联动流水的发生时间：当天 23:59，保证落在同一天
QDateTime investmentMoment(const QDate &date)
{
    return QDateTime(date, QTime(23, 59, 0));
}

}  // namespace

bool LedgerViewModel::isInvestmentAccount(int accountId) const
{
    const auto account = m_accountRepository.findById(accountId);
    return account.has_value() && isInvestmentAccountType(account->type);
}

QVariantList LedgerViewModel::dailyRecords(int accountId)
{
    QVariantList result;
    for (const AccountDailyRecord &record : m_dailyRecordRepository.listByAccount(accountId, 90)) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), record.id);
        item.insert(QStringLiteral("dateText"), investmentDateText(record.date));
        item.insert(QStringLiteral("incomeText"), record.income.format());
        item.insert(QStringLiteral("incomeCents"), record.income.cents());
        item.insert(QStringLiteral("commissionText"), record.commission.format());
        item.insert(QStringLiteral("marketValueText"),
                    record.marketValue.has_value() ? record.marketValue->format() : QString());
        item.insert(QStringLiteral("note"), record.note);
        result.append(item);
    }
    return result;
}

QVariantMap LedgerViewModel::investmentSummary(int accountId)
{
    QVariantMap result;
    const auto account = m_accountRepository.findById(accountId);
    if (!account.has_value())
        return result;

    const Money income = m_dailyRecordRepository.totalIncome(accountId);
    const Money commission = m_dailyRecordRepository.totalCommission(accountId);
    const QVector<AccountDailyRecord> records = m_dailyRecordRepository.listByAccount(accountId, 1);

    result.insert(QStringLiteral("name"), account->name);
    result.insert(QStringLiteral("typeText"), displayName(account->type));
    result.insert(QStringLiteral("openingText"), account->openingBalance.format());
    result.insert(QStringLiteral("incomeText"), income.formatSigned());
    result.insert(QStringLiteral("commissionText"), commission.format());
    result.insert(QStringLiteral("netText"), (income - commission).formatSigned());
    result.insert(QStringLiteral("count"), m_dailyRecordRepository.listByAccount(accountId, 999).size());
    result.insert(QStringLiteral("lastDateText"),
                  records.isEmpty() ? QString() : investmentDateText(records.first().date));
    result.insert(QStringLiteral("lastIncomeText"),
                  records.isEmpty() ? QString() : records.first().income.formatSigned());
    return result;
}

QVariantMap LedgerViewModel::saveDailyRecord(const QVariantMap &entry)
{
    const int accountId = entry.value(QStringLiteral("accountId"), -1).toInt();
    const auto account = m_accountRepository.findById(accountId);
    if (!account.has_value())
        return accountResult(false, QStringLiteral("账户不存在"));
    if (!isInvestmentAccountType(account->type))
        return accountResult(false, QStringLiteral("只有理财 / 股票 / 投资账户可以记录每日收益"));

    const QDate date = QDate::fromString(entry.value(QStringLiteral("dateText")).toString().trimmed(),
                                        QStringLiteral("yyyy-MM-dd"));
    if (!date.isValid())
        return accountResult(false, QStringLiteral("日期格式应为 yyyy-MM-dd"));

    const auto commission = entry.value(QStringLiteral("commissionText")).toString().trimmed().isEmpty()
                                ? Money::fromCents(0)
                                : Money::fromYuan(entry.value(QStringLiteral("commissionText")).toString());
    if (!commission.has_value() || commission->cents() < 0)
        return accountResult(false, QStringLiteral("佣金格式不正确"));

    const qint64 recordId = entry.value(QStringLiteral("recordId"), 0).toLongLong();
    std::optional<AccountDailyRecord> existing;
    if (recordId > 0)
        existing = m_dailyRecordRepository.findById(recordId);
    else
        existing = m_dailyRecordRepository.findByAccountAndDate(accountId, date);

    // 上一日市值：优先用最近一条记录的收盘市值，否则用账户期初余额
    Money previousValue = account->openingBalance;
    if (const auto previous = m_dailyRecordRepository.latestBefore(accountId, date);
        previous.has_value() && previous->marketValue.has_value()) {
        previousValue = *previous->marketValue;
    }

    const QString mode = entry.value(QStringLiteral("mode"), QStringLiteral("income")).toString();
    Money income;
    Money marketValue;
    if (mode == QLatin1String("value")) {
        const auto value = Money::fromYuan(entry.value(QStringLiteral("valueText")).toString());
        if (!value.has_value() || value->cents() < 0)
            return accountResult(false, QStringLiteral("市值格式不正确"));
        marketValue = *value;
        // 市值 = 上一日市值 + 当日收益 − 佣金  =>  收益 = 市值 − 上一日市值 + 佣金
        income = marketValue - previousValue + *commission;
    } else {
        const auto parsed = Money::fromYuan(entry.value(QStringLiteral("incomeText")).toString());
        if (!parsed.has_value())
            return accountResult(false, QStringLiteral("收益格式不正确"));
        income = *parsed;
        marketValue = previousValue + income - *commission;
    }

    AccountDailyRecord record;
    record.id = existing.has_value() ? existing->id : 0;
    record.accountId = accountId;
    record.date = date;
    record.income = income;
    record.commission = *commission;
    record.marketValue = marketValue;
    record.note = entry.value(QStringLiteral("note")).toString().trimmed();
    record.incomeTransactionId = existing.has_value() ? existing->incomeTransactionId : 0;
    record.commissionTransactionId = existing.has_value() ? existing->commissionTransactionId : 0;

    QString error;
    if (record.id > 0) {
        if (!m_dailyRecordRepository.update(record, &error))
            return accountResult(false, error.isEmpty() ? QStringLiteral("保存失败") : error);
    } else {
        const auto inserted = m_dailyRecordRepository.insert(record, &error);
        if (!inserted.has_value())
            return accountResult(false, error.isEmpty() ? QStringLiteral("保存失败") : error);
        record.id = *inserted;
    }

    // 联动：收益（正=收入，负=亏损支出）
    const auto incomeCategory = m_categoryRepository.findByName(
        income.cents() >= 0 ? QStringLiteral("投资收益") : QStringLiteral("投资亏损"),
        income.cents() >= 0 ? CategoryKind::Income : CategoryKind::Expense);
    const auto commissionCategory =
        m_categoryRepository.findByName(QStringLiteral("手续费"), CategoryKind::Expense);

    auto syncTransaction = [this](qint64 transactionId, TransactionKind kind, Money amount,
                                  int accountId, const std::optional<qint64> &categoryId,
                                  const QDateTime &moment, const QString &note) -> qint64 {
        if (amount.cents() <= 0) {
            if (transactionId > 0)
                m_transactionRepository.remove(transactionId);
            return 0;
        }
        Transaction transaction;
        transaction.kind = kind;
        transaction.amount = amount;
        transaction.accountId = accountId;
        transaction.categoryId = categoryId;
        transaction.occurredAt = moment;
        transaction.note = note;
        if (transactionId > 0) {
            transaction.id = transactionId;
            m_transactionRepository.update(transaction);
            return transactionId;
        }
        const auto inserted = m_transactionRepository.insert(transaction);
        return inserted.has_value() ? *inserted : 0;
    };

    record.incomeTransactionId =
        syncTransaction(record.incomeTransactionId,
                        income.cents() >= 0 ? TransactionKind::Income : TransactionKind::Expense,
                        Money::fromCents(std::abs(income.cents())),
                        accountId,
                        incomeCategory.has_value()
                            ? std::optional<qint64>(incomeCategory->id)
                            : std::nullopt,
                        investmentMoment(date),
                        QStringLiteral("每日收益 · %1").arg(account->name));
    record.commissionTransactionId =
        syncTransaction(record.commissionTransactionId, TransactionKind::Expense, *commission,
                        accountId,
                        commissionCategory.has_value()
                            ? std::optional<qint64>(commissionCategory->id)
                            : std::nullopt,
                        investmentMoment(date),
                        QStringLiteral("佣金 · %1").arg(account->name));
    m_dailyRecordRepository.update(record);

    reload();
    return accountResult(true,
                         QStringLiteral("%1 已记录：收益 %2，佣金 %3")
                             .arg(investmentDateText(date), income.formatSigned(),
                                  commission->format()));
}

QVariantMap LedgerViewModel::removeDailyRecord(qint64 recordId)
{
    const auto record = m_dailyRecordRepository.findById(recordId);
    if (!record.has_value())
        return accountResult(false, QStringLiteral("该记录不存在"));

    if (record->incomeTransactionId > 0)
        m_transactionRepository.remove(record->incomeTransactionId);
    if (record->commissionTransactionId > 0)
        m_transactionRepository.remove(record->commissionTransactionId);
    m_dailyRecordRepository.remove(recordId);
    reload();
    return accountResult(true, QStringLiteral("已删除该日收益记录（联动流水已同步删除）"));
}

QVariantMap LedgerViewModel::saveEntry(const QVariantMap &entry)
{
    const QString kind = entry.value(QStringLiteral("kind"), QStringLiteral("expense")).toString();
    const QString amountText = entry.value(QStringLiteral("amountText")).toString();
    const int accountId = entry.value(QStringLiteral("accountId"), -1).toInt();
    const int categoryId = entry.value(QStringLiteral("categoryId"), -1).toInt();
    const int toAccountId = entry.value(QStringLiteral("toAccountId"), -1).toInt();
    const QString dateText = entry.value(QStringLiteral("dateText")).toString();
    const QString timeText = entry.value(QStringLiteral("timeText")).toString();
    const QString note = entry.value(QStringLiteral("note")).toString();
    const QString tags = entry.value(QStringLiteral("tags")).toString();
    const QString nature = entry.value(QStringLiteral("transferNature"),
                                       QStringLiteral("transfer")).toString();

    const auto amount = Money::fromYuan(amountText);
    if (!amount.has_value() || amount->cents() <= 0)
        return accountResult(false, QStringLiteral("金额格式不正确"));
    if (accountId <= 0)
        return accountResult(false, QStringLiteral("请选择账户"));

    keep::Transaction transaction;
    transaction.amount = *amount;
    transaction.accountId = accountId;
    transaction.occurredAt = parseEntryMoment(dateText, timeText);
    transaction.note = note.trimmed();
    if (categoryId > 0)
        transaction.categoryId = categoryId;

    if (kind == QLatin1String("transfer")) {
        if (nature == QLatin1String("expense")) {
            // 转给他人 · 记作支出：按普通支出记录
            transaction.kind = TransactionKind::Expense;
            if (!transaction.categoryId.has_value()) {
                if (const auto loanCategory =
                        m_categoryRepository.findByName(QStringLiteral("借出"), CategoryKind::Expense);
                    loanCategory.has_value()) {
                    transaction.categoryId = loanCategory->id;
                }
            }
        } else {
            // 本账本互转 / 借出：都需要目标账户
            if (toAccountId <= 0)
                return accountResult(false, QStringLiteral("请选择转入账户"));
            if (toAccountId == accountId)
                return accountResult(false, QStringLiteral("转出与转入账户不能相同"));

            // 跨账本互转会让两侧净资产口径失真，直接拒绝
            const auto fromAccount = m_accountRepository.findById(accountId);
            const auto toAccount = m_accountRepository.findById(toAccountId);
            if (fromAccount.has_value() && toAccount.has_value()
                && fromAccount->ledgerId.has_value() && toAccount->ledgerId.has_value()
                && *fromAccount->ledgerId != *toAccount->ledgerId) {
                return accountResult(false,
                                     QStringLiteral("跨账本账户不能互转，请先把账户移到同一账本"));
            }
            transaction.kind = TransactionKind::Transfer;
            transaction.toAccountId = toAccountId;
            transaction.categoryId.reset();
        }
    } else if (kind == QLatin1String("income")) {
        transaction.kind = TransactionKind::Income;
    } else {
        transaction.kind = TransactionKind::Expense;
    }

    QString error;
    const auto id = m_transactionRepository.insert(transaction, &error);
    if (!id.has_value())
        return accountResult(false, error.isEmpty() ? QStringLiteral("保存失败") : error);

    const QStringList tagNames = splitTagText(tags);
    if (!tagNames.isEmpty())
        m_tagRepository.setTags(*id, tagNames);

    reload();

    auto inserted = std::make_shared<keep::Transaction>(transaction);
    inserted->id = *id;
    auto insertedId = std::make_shared<qint64>(*id);
    auto insertedTags = std::make_shared<QStringList>(tagNames);
    m_undoStack.push(new FunctionCommand(
        [this, inserted, insertedId, insertedTags] {
            const auto newId = m_transactionRepository.insert(*inserted);
            if (newId.has_value()) {
                *insertedId = *newId;
                inserted->id = *newId;
                if (!insertedTags->isEmpty())
                    m_tagRepository.setTags(*newId, *insertedTags);
            }
            reload();
        },
        [this, insertedId] {
            if (*insertedId > 0)
                m_transactionRepository.remove(*insertedId);
            reload();
        },
        QStringLiteral("记一笔")));

    QVariantMap result = accountResult(true, QStringLiteral("已保存，可 Ctrl+Z 撤销"));
    result.insert(QStringLiteral("id"), *id);
    return result;
}

QVariantMap LedgerViewModel::updateEntry(qint64 id, const QVariantMap &entry)
{
    const auto before = m_transactionRepository.findById(id);
    if (!before.has_value())
        return accountResult(false, QStringLiteral("这笔流水不存在"));

    const QString kind = entry.value(QStringLiteral("kind"), QStringLiteral("expense")).toString();
    const auto amount = Money::fromYuan(entry.value(QStringLiteral("amountText")).toString());
    if (!amount.has_value() || amount->cents() <= 0)
        return accountResult(false, QStringLiteral("金额格式不正确"));

    keep::Transaction after = *before;
    after.amount = *amount;
    after.accountId = entry.value(QStringLiteral("accountId"), before->accountId).toInt();
    after.occurredAt = parseEntryMoment(entry.value(QStringLiteral("dateText")).toString(),
                                        entry.value(QStringLiteral("timeText")).toString());
    after.note = entry.value(QStringLiteral("note")).toString().trimmed();

    const int categoryId = entry.value(QStringLiteral("categoryId"), -1).toInt();
    if (categoryId > 0)
        after.categoryId = categoryId;
    else
        after.categoryId.reset();

    if (kind == QLatin1String("transfer")) {
        const int toAccountId = entry.value(QStringLiteral("toAccountId"), -1).toInt();
        const QString nature = entry.value(QStringLiteral("transferNature"),
                                           QStringLiteral("transfer")).toString();
        if (nature == QLatin1String("expense")) {
            after.kind = TransactionKind::Expense;
            after.toAccountId.reset();
        } else {
            if (toAccountId <= 0 || toAccountId == after.accountId)
                return accountResult(false, QStringLiteral("请选择有效的转入账户"));
            after.kind = TransactionKind::Transfer;
            after.toAccountId = toAccountId;
            after.categoryId.reset();
        }
    } else if (kind == QLatin1String("income")) {
        after.kind = TransactionKind::Income;
        after.toAccountId.reset();
    } else {
        after.kind = TransactionKind::Expense;
        after.toAccountId.reset();
    }

    QString error;
    if (!m_transactionRepository.update(after, &error))
        return accountResult(false, error.isEmpty() ? QStringLiteral("更新失败") : error);

    const QStringList tagNames = splitTagText(entry.value(QStringLiteral("tags")).toString());
    m_tagRepository.setTags(id, tagNames);
    reload();

    auto beforeCopy = std::make_shared<keep::Transaction>(*before);
    auto afterCopy = std::make_shared<keep::Transaction>(after);
    auto beforeTags = std::make_shared<QStringList>(m_tagRepository.tagsFor(id));
    auto afterTags = std::make_shared<QStringList>(tagNames);
    m_undoStack.push(new FunctionCommand(
        [this, afterCopy, afterTags] {
            m_transactionRepository.update(*afterCopy);
            m_tagRepository.setTags(afterCopy->id, *afterTags);
            reload();
        },
        [this, beforeCopy, beforeTags] {
            m_transactionRepository.update(*beforeCopy);
            m_tagRepository.setTags(beforeCopy->id, *beforeTags);
            reload();
        },
        QStringLiteral("编辑一笔")));

    return accountResult(true, QStringLiteral("已更新，可 Ctrl+Z 撤销"));
}

QVariantMap LedgerViewModel::setTransactionTags(qint64 id, const QString &tags)
{
    QVariantMap result;
    if (!m_transactionRepository.findById(id).has_value()) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"), QStringLiteral("这笔流水不存在"));
        return result;
    }

    QString error;
    const QStringList names = splitTagText(tags);
    if (!m_tagRepository.setTags(id, names, &error)) {
        result.insert(QStringLiteral("ok"), false);
        result.insert(QStringLiteral("message"),
                      error.isEmpty() ? QStringLiteral("标签保存失败") : error);
        return result;
    }
    reload();

    result.insert(QStringLiteral("ok"), true);
    result.insert(QStringLiteral("message"),
                  names.isEmpty() ? QStringLiteral("已清空标签")
                                  : QStringLiteral("已设置 %1 个标签").arg(names.size()));
    return result;
}

QVariantMap LedgerViewModel::removeTransaction(qint64 id)
{
    QVariantMap result;
    const auto existing = m_transactionRepository.findById(id);
    QString error;
    const bool ok = m_transactionRepository.remove(id, &error);
    if (ok) {
        reload();
        if (existing.has_value()) {
            auto removed = std::make_shared<keep::Transaction>(*existing);
            auto removedTags = std::make_shared<QStringList>(
                m_tagRepository.tagsFor(removed->id));
            m_undoStack.push(new FunctionCommand(
                [this, removed] {
                    m_transactionRepository.remove(removed->id);
                    reload();
                },
                [this, removed, removedTags] {
                    const auto newId = m_transactionRepository.insert(*removed);
                    if (newId.has_value()) {
                        removed->id = *newId;
                        if (!removedTags->isEmpty())
                            m_tagRepository.setTags(*newId, *removedTags);
                    }
                    reload();
                },
                QStringLiteral("删除一笔")));
        }
    }

    result.insert(QStringLiteral("ok"), ok);
    result.insert(QStringLiteral("message"),
                  ok ? QStringLiteral("已删除")
                     : (error.isEmpty() ? QStringLiteral("删除失败") : error));
    return result;
}

}  // namespace keep