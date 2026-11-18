#include "core/data/LedgerRepository.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>

namespace keep {

namespace {

void setError(QString *error, const QSqlQuery &query)
{
    if (error)
        *error = query.lastError().text();
}

Ledger ledgerFromQuery(const QSqlQuery &query)
{
    Ledger ledger;
    ledger.id = query.value(0).toLongLong();
    ledger.name = query.value(1).toString();
    ledger.color = query.value(2).toString();
    ledger.isDefault = query.value(3).toInt() != 0;
    ledger.sortOrder = query.value(4).toInt();
    ledger.accountCount = query.value(5).toInt();
    return ledger;
}

}  // namespace

LedgerRepository::LedgerRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Ledger> LedgerRepository::listAll(QString *error) const
{
    QVector<Ledger> rows;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "SELECT l.id, l.name, l.color, l.is_default, l.sort_order, "
            "(SELECT COUNT(*) FROM accounts a WHERE a.ledger_id = l.id) "
            "FROM ledgers l ORDER BY l.is_default DESC, l.sort_order, l.id"))) {
        setError(error, query);
        return rows;
    }
    while (query.next())
        rows.append(ledgerFromQuery(query));
    return rows;
}

std::optional<qint64> LedgerRepository::insert(const QString &name, const QString &color,
                                               QString *error)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = QStringLiteral("账本名称不能为空");
        return std::nullopt;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO ledgers(name, color, is_default, sort_order, created_at) "
        "VALUES (?, ?, 0, (SELECT COALESCE(MAX(sort_order), 0) + 1 FROM ledgers), ?)"));
    query.addBindValue(trimmed);
    query.addBindValue(color);
    query.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")));
    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    return query.lastInsertId().toLongLong();
}

bool LedgerRepository::rename(qint64 id, const QString &name, QString *error)
{
    if (name.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("账本名称不能为空");
        return false;
    }
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE ledgers SET name = ? WHERE id = ?"));
    query.addBindValue(name.trimmed());
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

bool LedgerRepository::remove(qint64 id, QString *error)
{
    QSqlQuery check(m_db);
    check.prepare(QStringLiteral("SELECT is_default FROM ledgers WHERE id = ?"));
    check.addBindValue(id);
    if (!check.exec() || !check.next()) {
        if (error)
            *error = QStringLiteral("账本不存在");
        return false;
    }
    if (check.value(0).toInt() != 0) {
        if (error)
            *error = QStringLiteral("默认账本不能删除");
        return false;
    }

    QSqlQuery accounts(m_db);
    accounts.prepare(QStringLiteral("SELECT COUNT(*) FROM accounts WHERE ledger_id = ?"));
    accounts.addBindValue(id);
    if (accounts.exec() && accounts.next() && accounts.value(0).toInt() > 0) {
        if (error)
            *error = QStringLiteral("该账本下还有账户，请先移动或删除账户");
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM ledgers WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

std::optional<qint64> LedgerRepository::defaultId(QString *error) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT id FROM ledgers ORDER BY is_default DESC, id LIMIT 1"))) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return query.value(0).toLongLong();
}

}  // namespace keep