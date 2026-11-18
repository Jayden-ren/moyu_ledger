#include "core/data/AccountRepository.h"

#include "core/data/Mapping.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace keep {

namespace {

constexpr auto kSelectColumns = "id, name, type, currency, opening_balance_cents, ledger_id, "
                                "credit_limit_cents, icon, color, sort_order, is_archived";

// 账户余额 = 期初 + Σ(收入) − Σ(支出/转出) + Σ(转入)
constexpr auto kBalanceExpression =
    "a.opening_balance_cents"
    " + COALESCE((SELECT SUM(CASE t.kind WHEN 'income' THEN t.amount_cents"
    "             ELSE -t.amount_cents END)"
    "             FROM transactions t WHERE t.account_id = a.id), 0)"
    " + COALESCE((SELECT SUM(t.amount_cents) FROM transactions t"
    "             WHERE t.to_account_id = a.id AND t.kind = 'transfer'), 0)";

void setError(QString *error, const QSqlQuery &query)
{
    if (error)
        *error = query.lastError().text();
}

}  // namespace

AccountRepository::AccountRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Account> AccountRepository::listAll(bool includeArchived, QString *error) const
{
    QVector<Account> accounts;
    QSqlQuery query(m_db);

    QString where = includeArchived ? QString() : QStringLiteral("WHERE is_archived = 0");
    if (m_ledgerId.has_value())
        where += (where.isEmpty() ? QStringLiteral("WHERE ") : QStringLiteral(" AND "))
                 + QStringLiteral("ledger_id = ?");

    const QString sql = QStringLiteral("SELECT %1 FROM accounts %2 ORDER BY sort_order, id")
                            .arg(QString::fromLatin1(kSelectColumns), where);
    query.prepare(sql);
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec()) {
        setError(error, query);
        return accounts;
    }

    while (query.next())
        accounts.append(detail::accountFromQuery(query));
    return accounts;
}

QVector<AccountWithBalance> AccountRepository::listWithBalances(bool includeArchived,
                                                                QString *error) const
{
    QVector<AccountWithBalance> rows;
    QSqlQuery query(m_db);

    QString where = includeArchived ? QString() : QStringLiteral("WHERE a.is_archived = 0");
    if (m_ledgerId.has_value())
        where += (where.isEmpty() ? QStringLiteral("WHERE ") : QStringLiteral(" AND "))
                 + QStringLiteral("a.ledger_id = ?");

    const QString sql =
        QStringLiteral("SELECT a.id, a.name, a.type, a.currency, a.opening_balance_cents, "
                       "a.credit_limit_cents, a.icon, a.color, a.sort_order, a.is_archived, a.ledger_id, "
                       "(%1) AS balance_cents FROM accounts a %2 ORDER BY a.sort_order, a.id")
            .arg(QString::fromLatin1(kBalanceExpression), where);
    query.prepare(sql);
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec()) {
        setError(error, query);
        return rows;
    }

    while (query.next()) {
        AccountWithBalance row;
        row.account = detail::accountFromQuery(query);
        row.balance = Money::fromCents(query.value(QStringLiteral("balance_cents")).toLongLong());
        rows.append(row);
    }
    return rows;
}

std::optional<Account> AccountRepository::findById(qint64 id, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM accounts WHERE id = ?")
                      .arg(QString::fromLatin1(kSelectColumns)));
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;

    return detail::accountFromQuery(query);
}

std::optional<Money> AccountRepository::balance(qint64 id, QString *error) const
{
    QSqlQuery query(m_db);
    const QString sql =
        QStringLiteral("SELECT (%1) AS balance_cents FROM accounts a WHERE a.id = ?")
            .arg(QString::fromLatin1(kBalanceExpression));
    query.prepare(sql);
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;

    return Money::fromCents(query.value(QStringLiteral("balance_cents")).toLongLong());
}

std::optional<Money> AccountRepository::netWorth(QString *error) const
{
    QSqlQuery query(m_db);

    const QString ledgerClause =
        m_ledgerId.has_value() ? QStringLiteral(" AND a.ledger_id = ?") : QString();
    // 应收（receivable）计入资产；应付（payable）按负债扣减
    const QString sql =
        QStringLiteral("SELECT COALESCE(SUM(CASE WHEN a.type = 'payable' THEN -(%1) ELSE (%1) END), 0) "
                       "FROM accounts a WHERE a.is_archived = 0%2")
            .arg(QString::fromLatin1(kBalanceExpression), ledgerClause);
    query.prepare(sql);
    if (m_ledgerId.has_value())
        query.addBindValue(*m_ledgerId);

    if (!query.exec() || !query.next()) {
        setError(error, query);
        return std::nullopt;
    }
    return Money::fromCents(query.value(0).toLongLong());
}

std::optional<qint64> AccountRepository::insert(const Account &account, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO accounts(name, type, currency, opening_balance_cents, credit_limit_cents, "
        "icon, color, sort_order, is_archived, ledger_id, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, "
        "COALESCE(?, (SELECT id FROM ledgers WHERE is_default = 1 LIMIT 1)), ?)"));
    query.addBindValue(account.name);
    query.addBindValue(toDbString(account.type));
    query.addBindValue(account.currency);
    query.addBindValue(account.openingBalance.cents());
    query.addBindValue(account.creditLimitCents ? QVariant(*account.creditLimitCents) : QVariant());
    query.addBindValue(account.icon);
    query.addBindValue(account.color);
    query.addBindValue(account.sortOrder);
    query.addBindValue(account.archived ? 1 : 0);
    query.addBindValue(account.ledgerId.has_value() ? QVariant(*account.ledgerId) : QVariant());
    query.addBindValue(detail::timestampText(QDateTime::currentDateTime()));

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    return query.lastInsertId().toLongLong();
}

bool AccountRepository::update(const Account &account, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE accounts SET name = ?, type = ?, currency = ?, opening_balance_cents = ?, "
        "credit_limit_cents = ?, icon = ?, color = ?, sort_order = ?, is_archived = ?, "
        "ledger_id = COALESCE(?, ledger_id) WHERE id = ?"));
    query.addBindValue(account.name);
    query.addBindValue(toDbString(account.type));
    query.addBindValue(account.currency);
    query.addBindValue(account.openingBalance.cents());
    query.addBindValue(account.creditLimitCents ? QVariant(*account.creditLimitCents) : QVariant());
    query.addBindValue(account.icon);
    query.addBindValue(account.color);
    query.addBindValue(account.sortOrder);
    query.addBindValue(account.archived ? 1 : 0);
    query.addBindValue(account.ledgerId.has_value() ? QVariant(*account.ledgerId) : QVariant());
    query.addBindValue(account.id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool AccountRepository::setArchived(qint64 id, bool archived, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE accounts SET is_archived = ? WHERE id = ?"));
    query.addBindValue(archived ? 1 : 0);
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.numRowsAffected() > 0;
}

}  // namespace keep