#include "core/data/TransactionRepository.h"

#include "core/data/Mapping.h"

#include <QDate>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace keep {

namespace {

constexpr auto kSelectColumns = "id, kind, amount_cents, account_id, to_account_id, category_id, "
                                "occurred_at, note, payee, external_id";

void setError(QString *error, const QSqlQuery &query)
{
    if (error)
        *error = query.lastError().text();
}

// 空 QString 默认是 null，绑定会写成 NULL 并撞上 NOT NULL 约束，这里统一归一为空串
QVariant textValue(const QString &text)
{
    return text.isNull() ? QStringLiteral("") : text;
}

}  // namespace

TransactionRepository::TransactionRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QString TransactionRepository::monthStartText(int year, int month)
{
    return QDate(year, month, 1).toString(QStringLiteral("yyyy-MM-dd")) + QStringLiteral("T00:00:00");
}

QString TransactionRepository::monthEndText(int year, int month)
{
    return QDate(year, month, 1)
               .addMonths(1)
               .toString(QStringLiteral("yyyy-MM-dd"))
           + QStringLiteral("T00:00:00");
}

std::optional<qint64> TransactionRepository::insert(const Transaction &transaction, QString *error)
{
    const QString now = detail::timestampText(QDateTime::currentDateTime());

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO transactions(kind, amount_cents, account_id, to_account_id, category_id, "
        "occurred_at, note, payee, external_id, created_at, updated_at) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(toDbString(transaction.kind));
    query.addBindValue(transaction.amount.cents());
    query.addBindValue(transaction.accountId);
    query.addBindValue(transaction.toAccountId ? QVariant(*transaction.toAccountId) : QVariant());
    query.addBindValue(transaction.categoryId ? QVariant(*transaction.categoryId) : QVariant());
    query.addBindValue(detail::timestampText(transaction.occurredAt));
    query.addBindValue(textValue(transaction.note));
    query.addBindValue(textValue(transaction.payee));
    query.addBindValue(transaction.externalId.isEmpty() ? QVariant() : QVariant(transaction.externalId));
    query.addBindValue(now);
    query.addBindValue(now);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    return query.lastInsertId().toLongLong();
}

bool TransactionRepository::update(const Transaction &transaction, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE transactions SET kind = ?, amount_cents = ?, account_id = ?, to_account_id = ?, "
        "category_id = ?, occurred_at = ?, note = ?, payee = ?, external_id = ?, updated_at = ? WHERE id = ?"));
    query.addBindValue(toDbString(transaction.kind));
    query.addBindValue(transaction.amount.cents());
    query.addBindValue(transaction.accountId);
    query.addBindValue(transaction.toAccountId ? QVariant(*transaction.toAccountId) : QVariant());
    query.addBindValue(transaction.categoryId ? QVariant(*transaction.categoryId) : QVariant());
    query.addBindValue(detail::timestampText(transaction.occurredAt));
    query.addBindValue(textValue(transaction.note));
    query.addBindValue(textValue(transaction.payee));
    query.addBindValue(transaction.externalId.isEmpty() ? QVariant() : QVariant(transaction.externalId));
    query.addBindValue(detail::timestampText(QDateTime::currentDateTime()));
    query.addBindValue(transaction.id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool TransactionRepository::remove(qint64 id, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM transactions WHERE id = ?"));
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.numRowsAffected() > 0;
}

std::optional<Transaction> TransactionRepository::findById(qint64 id, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM transactions WHERE id = ?")
                      .arg(QString::fromLatin1(kSelectColumns)));
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return detail::transactionFromQuery(query);
}

namespace {

void appendFilter(QString &sql, QList<QVariant> &values, const TransactionFilter &filter,
                  const QVector<Category> &categories)
{
    Q_UNUSED(categories)

    const QString keyword = filter.keyword.trimmed();
    if (!keyword.isEmpty()) {
        const bool numeric = Money::fromYuan(keyword).has_value();
        QString clause = QStringLiteral(" AND (note LIKE ? OR payee LIKE ?");
        values.append(QStringLiteral("%%%1%%").arg(keyword));
        values.append(QStringLiteral("%%%1%%").arg(keyword));
        if (numeric) {
            clause += QStringLiteral(" OR amount_cents = ?");
            values.append(Money::fromYuan(keyword)->cents());
        } else {
            // 分类名也参与关键字匹配
            clause += QStringLiteral(
                " OR category_id IN (SELECT id FROM categories WHERE name LIKE ?)");
            values.append(QStringLiteral("%%%1%%").arg(keyword));
        }
        sql += clause + QStringLiteral(")");
    }

    if (filter.accountId.has_value()) {
        sql += QStringLiteral(" AND account_id = ?");
        values.append(*filter.accountId);
    }

    if (filter.categoryId.has_value()) {
        sql += QStringLiteral(" AND category_id = ?");
        values.append(*filter.categoryId);
    }
}

}  // namespace

QVector<Transaction> TransactionRepository::listByMonth(int year, int month,
                                                        const TransactionFilter &filter,
                                                        QString *error) const
{
    QVector<Transaction> transactions;
    QSqlQuery query(m_db);

    QString sql = QStringLiteral("SELECT %1 FROM transactions WHERE occurred_at >= ? "
                                 "AND occurred_at < ?")
                      .arg(QString::fromLatin1(kSelectColumns));
    QList<QVariant> values;
    values.append(monthStartText(year, month));
    values.append(monthEndText(year, month));

    appendFilter(sql, values, filter, {});

    if (m_ledgerId.has_value()) {
        sql += QStringLiteral(" AND account_id IN (SELECT id FROM accounts WHERE ledger_id = ?)");
        values.append(*m_ledgerId);
    }

    sql += QStringLiteral(" ORDER BY occurred_at DESC, id DESC");

    query.prepare(sql);
    for (const QVariant &value : values)
        query.addBindValue(value);

    if (!query.exec()) {
        setError(error, query);
        return transactions;
    }

    while (query.next())
        transactions.append(detail::transactionFromQuery(query));
    return transactions;
}

QVector<Transaction> TransactionRepository::listByMonth(int year, int month, QString *error) const
{
    return listByMonth(year, month, TransactionFilter{}, error);
}

QVector<AccountTotal> TransactionRepository::accountTotals(int year, int month,
                                                           QString *error) const
{
    QVector<AccountTotal> totals;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT a.id, a.name, a.color, "
        "COALESCE(SUM(CASE WHEN t.kind = 'income' THEN t.amount_cents ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN t.kind = 'expense' THEN t.amount_cents ELSE 0 END), 0), "
        "COUNT(t.id) "
        "FROM accounts a LEFT JOIN transactions t "
        "  ON t.account_id = a.id AND t.occurred_at >= ? AND t.occurred_at < ?%1 "
        "GROUP BY a.id ORDER BY a.sort_order, a.id")
            .arg(m_ledgerId.has_value()
                     ? QStringLiteral(" AND a.ledger_id = ?")
                     : QString()));
    query.addBindValue(monthStartText(year, month));
    query.addBindValue(monthEndText(year, month));
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec()) {
        setError(error, query);
        return totals;
    }

    while (query.next()) {
        AccountTotal total;
        total.accountId = query.value(0).toLongLong();
        total.name = query.value(1).toString();
        total.color = query.value(2).toString();
        total.income = Money::fromCents(query.value(3).toLongLong());
        total.expense = Money::fromCents(query.value(4).toLongLong());
        total.count = query.value(5).toInt();
        totals.append(total);
    }
    return totals;
}

MonthSummary TransactionRepository::monthSummary(int year, int month, QString *error) const
{
    MonthSummary summary;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT COALESCE(SUM(CASE WHEN kind = 'income' THEN amount_cents ELSE 0 END), 0), "
        "COALESCE(SUM(CASE WHEN kind = 'expense' THEN amount_cents ELSE 0 END), 0), COUNT(*) "
        "FROM transactions WHERE occurred_at >= ? AND occurred_at < ?%1")
            .arg(m_ledgerId.has_value()
                     ? QStringLiteral(" AND account_id IN (SELECT id FROM accounts WHERE ledger_id = ?)")
                     : QString()));
    query.addBindValue(monthStartText(year, month));
    query.addBindValue(monthEndText(year, month));
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec() || !query.next()) {
        setError(error, query);
        return summary;
    }

    summary.income = Money::fromCents(query.value(0).toLongLong());
    summary.expense = Money::fromCents(query.value(1).toLongLong());
    summary.count = query.value(2).toInt();
    return summary;
}

QVector<CategoryTotal> TransactionRepository::categoryShares(int year, int month,
                                                             TransactionKind kind,
                                                             QString *error) const
{
    QVector<CategoryTotal> totals;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT c.id, c.name, c.color, SUM(t.amount_cents) AS total_cents "
        "FROM transactions t JOIN categories c ON c.id = t.category_id "
        "WHERE t.kind = ? AND t.occurred_at >= ? AND t.occurred_at < ?%1 "
        "GROUP BY c.id ORDER BY total_cents DESC")
            .arg(m_ledgerId.has_value()
                     ? QStringLiteral(" AND t.account_id IN (SELECT id FROM accounts WHERE ledger_id = ?)")
                     : QString()));
    query.addBindValue(toDbString(kind));
    query.addBindValue(monthStartText(year, month));
    query.addBindValue(monthEndText(year, month));
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec()) {
        setError(error, query);
        return totals;
    }

    qint64 sum = 0;
    while (query.next()) {
        CategoryTotal total;
        total.categoryId = query.value(0).toLongLong();
        total.name = query.value(1).toString();
        total.color = query.value(2).toString();
        total.total = Money::fromCents(query.value(3).toLongLong());
        sum += total.total.cents();
        totals.append(total);
    }

    if (sum > 0) {
        for (CategoryTotal &total : totals)
            total.share = static_cast<double>(total.total.cents()) / static_cast<double>(sum);
    }
    return totals;
}

QVector<MonthPoint> TransactionRepository::monthlyTrend(int endYear, int endMonth, int monthCount,
                                                        QString *error) const
{
    QVector<MonthPoint> points;
    if (monthCount <= 0)
        return points;

    const QDate end(endYear, endMonth, 1);
    const QDate begin = end.addMonths(-(monthCount - 1));

    for (int i = 0; i < monthCount; ++i) {
        const QDate month = begin.addMonths(i);
        MonthPoint point;
        point.year = month.year();
        point.month = month.month();
        points.append(point);
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT strftime('%Y-%m', occurred_at) AS ym, "
        "SUM(CASE WHEN kind = 'income' THEN amount_cents ELSE 0 END), "
        "SUM(CASE WHEN kind = 'expense' THEN amount_cents ELSE 0 END) "
        "FROM transactions WHERE occurred_at >= ? AND occurred_at < ?%1 GROUP BY ym")
            .arg(m_ledgerId.has_value()
                     ? QStringLiteral(" AND account_id IN (SELECT id FROM accounts WHERE ledger_id = ?)")
                     : QString()));
    query.addBindValue(monthStartText(begin.year(), begin.month()));
    query.addBindValue(monthEndText(end.year(), end.month()));
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec()) {
        setError(error, query);
        return points;
    }

    while (query.next()) {
        const QString key = query.value(0).toString();
        const int year = key.left(4).toInt();
        const int month = key.mid(5, 2).toInt();
        for (MonthPoint &point : points) {
            if (point.year == year && point.month == month) {
                point.income = Money::fromCents(query.value(1).toLongLong());
                point.expense = Money::fromCents(query.value(2).toLongLong());
                break;
            }
        }
    }
    return points;
}

bool TransactionRepository::hasExternalId(const QString &externalId, QString *error) const
{
    if (externalId.isEmpty())
        return false;

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT 1 FROM transactions WHERE external_id = ? LIMIT 1"));
    query.addBindValue(externalId);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.next();
}

int TransactionRepository::countAll(QString *error) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM transactions")) || !query.next()) {
        setError(error, query);
        return -1;
    }
    return query.value(0).toInt();
}

}  // namespace keep