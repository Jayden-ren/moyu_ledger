#include "core/model/TransactionListModel.h"

#include <QLocale>

namespace keep {

namespace {

const char *const kWeekdays[] = {"", "周一", "周二", "周三", "周四", "周五", "周六", "周日"};

}  // namespace

TransactionListModel::TransactionListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int TransactionListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_rows.size());
}

QString TransactionListModel::occurredText(const QDateTime &value) const
{
    return value.toString(QStringLiteral("HH:mm"));
}

QVariant TransactionListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const Row &row = m_rows.at(index.row());
    const Money &amount = row.transaction.amount;

    switch (role) {
    case IdRole:
        return row.transaction.id;
    case KindRole:
        return toDbString(row.transaction.kind);
    case KindTextRole:
        return displayName(row.transaction.kind);
    case AmountTextRole:
        return row.transaction.kind == TransactionKind::Income ? QStringLiteral("+") + amount.format()
                                                               : QStringLiteral("-") + amount.format();
    case AmountCentsRole:
        return static_cast<qint64>(signedCents(row.transaction.kind, amount));
    case IsExpenseRole:
        return row.transaction.kind == TransactionKind::Expense;
    case CategoryIdRole:
        return row.transaction.categoryId.value_or(0);
    case CategoryNameRole:
        return row.categoryName;
    case CategoryBadgeRole:
        return row.categoryBadge;
    case CategoryColorRole:
        return row.categoryColor;
    case AccountIdRole:
        return row.transaction.accountId;
    case AccountNameRole:
        return row.accountName;
    case OccurredTextRole:
        return occurredText(row.transaction.occurredAt);
    case OccurredFullTextRole:
        return row.transaction.occurredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    case DateTextRole:
        return row.transaction.occurredAt.toString(QStringLiteral("yyyy-MM-dd HH:mm"));
    case DateKeyRole:
        return row.dateKey;
    case NoteRole:
        return row.transaction.note;
    case PayeeRole:
        return row.transaction.payee;
    case TagsTextRole:
        return row.tagsText;
    case ToAccountIdRole:
        return row.transaction.toAccountId.has_value()
                   ? QVariant(*row.transaction.toAccountId)
                   : QVariant(-1);
    case ToAccountNameRole:
        return row.toAccountName;
    default:
        return {};
    }
}

QHash<int, QByteArray> TransactionListModel::roleNames() const
{
    return {
        {IdRole, "transactionId"},
        {KindRole, "kind"},
        {KindTextRole, "kindText"},
        {AmountTextRole, "amountText"},
        {AmountCentsRole, "amountCents"},
        {IsExpenseRole, "isExpense"},
        {CategoryIdRole, "categoryId"},
        {CategoryNameRole, "categoryName"},
        {CategoryBadgeRole, "categoryBadge"},
        {CategoryColorRole, "categoryColor"},
        {AccountIdRole, "accountId"},
        {AccountNameRole, "accountName"},
        {OccurredTextRole, "occurredText"},
        {OccurredFullTextRole, "occurredFullText"},
        {DateTextRole, "dateText"},
        {DateKeyRole, "dateKey"},
        {NoteRole, "note"},
        {PayeeRole, "payee"},
        {TagsTextRole, "tagsText"},
        {ToAccountIdRole, "toAccountId"},
        {ToAccountNameRole, "toAccountName"},
    };
}

void TransactionListModel::setTransactions(
    QVector<Transaction> transactions,
    const QHash<qint64, QString> &accountNames,
    const QHash<qint64, QString> &categoryNames,
    const QHash<qint64, QPair<QString, QString>> &categoryBadges,
    const QHash<qint64, QString> &tagTexts)
{
    beginResetModel();
    m_rows.clear();
    m_dayIncome.clear();
    m_dayExpense.clear();
    m_dayLabels.clear();

    const QDate today = QDate::currentDate();

    for (const Transaction &transaction : transactions) {
        Row row;
        row.transaction = transaction;
        row.accountName = accountNames.value(transaction.accountId);
        if (transaction.categoryId.has_value()) {
            row.categoryName = categoryNames.value(*transaction.categoryId);
            const auto badge = categoryBadges.value(*transaction.categoryId);
            row.categoryBadge = badge.first;
            row.categoryColor = badge.second;
        }
        if (row.categoryBadge.isEmpty() && !row.categoryName.isEmpty())
            row.categoryBadge = row.categoryName.left(1);

        row.dateKey = transaction.occurredAt.date().toString(QStringLiteral("yyyy-MM-dd"));
        row.tagsText = tagTexts.value(transaction.id);
        if (transaction.toAccountId.has_value())
            row.toAccountName = accountNames.value(*transaction.toAccountId);

        if (!m_dayLabels.contains(row.dateKey)) {
            const QDate date = transaction.occurredAt.date();
            const QString prefix = (date == today) ? QStringLiteral("今天 ") : QString();
            m_dayLabels.insert(row.dateKey,
                               prefix + date.toString(QStringLiteral("MM-dd ")) + QString::fromUtf8(kWeekdays[date.dayOfWeek()]));
        }

        // 转账不计收支，因此也不计入日分组合计
        if (transaction.kind == TransactionKind::Income)
            m_dayIncome[row.dateKey] += transaction.amount.cents();
        else if (transaction.kind == TransactionKind::Expense)
            m_dayExpense[row.dateKey] += transaction.amount.cents();
        m_rows.append(row);
    }

    endResetModel();
    emit countChanged();
}

QVariantMap TransactionListModel::rowData(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;

    const QModelIndex modelIndex = index(row, 0);
    // 必须绑定到局部变量：roleNames() 每次返回一个新的 QHash，
    // 若 cbegin()/cend() 各取一次临时对象，迭代器属于不同容器 → 未定义行为（会死循环）
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it)
        map.insert(QString::fromUtf8(it.value()), data(modelIndex, it.key()));

    map.insert(QStringLiteral("row"), row);
    return map;
}

QString TransactionListModel::dayLabel(const QString &dateKey) const
{
    return m_dayLabels.value(dateKey);
}

QString TransactionListModel::dayTotalText(const QString &dateKey) const
{
    const qint64 income = m_dayIncome.value(dateKey);
    const qint64 expense = m_dayExpense.value(dateKey);

    if (income > 0 && expense > 0) {
        return QStringLiteral("收入 %1 · 支出 %2")
            .arg(Money::fromCents(income).formatSigned(), Money::fromCents(expense).format());
    }
    if (income > 0)
        return QStringLiteral("收入 %1").arg(Money::fromCents(income).formatSigned());
    if (expense > 0)
        return QStringLiteral("支出 %1").arg(Money::fromCents(expense).format());
    return QString();
}

}  // namespace keep