#include "core/data/AccountDailyRecordRepository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace keep {
namespace {

void setError(QString *error, const QSqlQuery &query)
{
    if (error)
        *error = query.lastError().text();
}

QString dateText(const QDate &date)
{
    return date.toString(QStringLiteral("yyyy-MM-dd"));
}

AccountDailyRecord readRow(const QSqlQuery &query)
{
    AccountDailyRecord record;
    record.id = query.value(0).toLongLong();
    record.accountId = query.value(1).toLongLong();
    record.date = QDate::fromString(query.value(2).toString(), QStringLiteral("yyyy-MM-dd"));
    record.income = Money::fromCents(query.value(3).toLongLong());
    record.commission = Money::fromCents(query.value(4).toLongLong());
    if (!query.value(5).isNull())
        record.marketValue = Money::fromCents(query.value(5).toLongLong());
    record.note = query.value(6).toString();
    record.incomeTransactionId = query.value(7).toLongLong();
    record.commissionTransactionId = query.value(8).toLongLong();
    return record;
}

const char *kSelect = "SELECT id, account_id, occurred_on, income_cents, commission_cents, "
                      "market_value_cents, note, income_tx_id, commission_tx_id "
                      "FROM account_daily_records ";

}  // namespace

AccountDailyRecordRepository::AccountDailyRecordRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

std::optional<AccountDailyRecord> AccountDailyRecordRepository::findByAccountAndDate(
    qint64 accountId, const QDate &date, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSelect)
                  + QStringLiteral("WHERE account_id = ? AND occurred_on = ?"));
    query.addBindValue(accountId);
    query.addBindValue(dateText(date));
    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return readRow(query);
}

std::optional<AccountDailyRecord> AccountDailyRecordRepository::findById(qint64 id, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSelect) + QStringLiteral("WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return readRow(query);
}

QVector<AccountDailyRecord> AccountDailyRecordRepository::listByAccount(qint64 accountId, int limit,
                                                                       QString *error) const
{
    QVector<AccountDailyRecord> records;
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSelect)
                  + QStringLiteral("WHERE account_id = ? ORDER BY occurred_on DESC LIMIT ?"));
    query.addBindValue(accountId);
    query.addBindValue(limit);
    if (!query.exec()) {
        setError(error, query);
        return records;
    }
    while (query.next())
        records.append(readRow(query));
    return records;
}

std::optional<AccountDailyRecord> AccountDailyRecordRepository::latestBefore(qint64 accountId,
                                                                             const QDate &date,
                                                                             QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QString::fromLatin1(kSelect)
                  + QStringLiteral("WHERE account_id = ? AND occurred_on < ? "
                                   "ORDER BY occurred_on DESC LIMIT 1"));
    query.addBindValue(accountId);
    query.addBindValue(dateText(date));
    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return readRow(query);
}

Money AccountDailyRecordRepository::totalIncome(qint64 accountId, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT COALESCE(SUM(income_cents), 0) "
                                 "FROM account_daily_records WHERE account_id = ?"));
    query.addBindValue(accountId);
    if (!query.exec() || !query.next()) {
        setError(error, query);
        return Money::fromCents(0);
    }
    return Money::fromCents(query.value(0).toLongLong());
}

Money AccountDailyRecordRepository::totalCommission(qint64 accountId, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT COALESCE(SUM(commission_cents), 0) "
                                 "FROM account_daily_records WHERE account_id = ?"));
    query.addBindValue(accountId);
    if (!query.exec() || !query.next()) {
        setError(error, query);
        return Money::fromCents(0);
    }
    return Money::fromCents(query.value(0).toLongLong());
}

std::optional<qint64> AccountDailyRecordRepository::insert(const AccountDailyRecord &record,
                                                           QString *error)
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO account_daily_records(account_id, occurred_on, income_cents, commission_cents, "
        "market_value_cents, note, income_tx_id, commission_tx_id, created_at, updated_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(record.accountId);
    query.addBindValue(dateText(record.date));
    query.addBindValue(record.income.cents());
    query.addBindValue(record.commission.cents());
    query.addBindValue(record.marketValue.has_value() ? QVariant(record.marketValue->cents())
                                                      : QVariant());
    query.addBindValue(record.note.isEmpty() ? QVariant() : QVariant(record.note));
    query.addBindValue(record.incomeTransactionId > 0 ? QVariant(record.incomeTransactionId)
                                                      : QVariant());
    query.addBindValue(record.commissionTransactionId > 0 ? QVariant(record.commissionTransactionId)
                                                          : QVariant());
    query.addBindValue(now);
    query.addBindValue(now);
    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    return query.lastInsertId().toLongLong();
}

bool AccountDailyRecordRepository::update(const AccountDailyRecord &record, QString *error)
{
    const QString now = QDateTime::currentDateTime().toString(Qt::ISODate);
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE account_daily_records SET income_cents = ?, commission_cents = ?, "
        "market_value_cents = ?, note = ?, income_tx_id = ?, commission_tx_id = ?, updated_at = ? "
        "WHERE id = ?"));
    query.addBindValue(record.income.cents());
    query.addBindValue(record.commission.cents());
    query.addBindValue(record.marketValue.has_value() ? QVariant(record.marketValue->cents())
                                                      : QVariant());
    query.addBindValue(record.note.isEmpty() ? QVariant() : QVariant(record.note));
    query.addBindValue(record.incomeTransactionId > 0 ? QVariant(record.incomeTransactionId)
                                                      : QVariant());
    query.addBindValue(record.commissionTransactionId > 0 ? QVariant(record.commissionTransactionId)
                                                          : QVariant());
    query.addBindValue(now);
    query.addBindValue(record.id);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

bool AccountDailyRecordRepository::remove(qint64 id, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM account_daily_records WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

}  // namespace keep