#include "core/data/RecurringRepository.h"

#include "core/data/TransactionRepository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>

namespace keep {

namespace {

void setError(QString *error, const QSqlQuery &query)
{
    if (error)
        *error = query.lastError().text();
}

QString monthKey(int year, int month)
{
    return QStringLiteral("%1-%2").arg(year).arg(month, 2, 10, QLatin1Char('0'));
}

RecurringRule ruleFromQuery(const QSqlQuery &query)
{
    RecurringRule rule;
    rule.id = query.value(0).toLongLong();
    rule.kind = query.value(1).toString() == QLatin1String("income")
                    ? TransactionKind::Income
                    : TransactionKind::Expense;
    rule.amount = Money::fromCents(query.value(2).toLongLong());
    rule.accountId = query.value(3).toLongLong();
    if (!query.value(4).isNull())
        rule.categoryId = query.value(4).toLongLong();
    rule.note = query.value(5).toString();
    rule.dayOfMonth = query.value(6).toInt();
    rule.enabled = query.value(7).toInt() != 0;
    rule.lastGeneratedMonth = query.value(8).toString();
    return rule;
}

constexpr auto kSelect = "id, kind, amount_cents, account_id, category_id, note, day_of_month, "
                         "enabled, last_generated_month";

}  // namespace

RecurringRepository::RecurringRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<RecurringRule> RecurringRepository::listAll(QString *error) const
{
    QVector<RecurringRule> rules;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT %1 FROM recurring_rules ORDER BY day_of_month, id")
                        .arg(QString::fromLatin1(kSelect)))) {
        setError(error, query);
        return rules;
    }
    while (query.next())
        rules.append(ruleFromQuery(query));
    return rules;
}

std::optional<qint64> RecurringRepository::insert(const RecurringRule &rule, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO recurring_rules(kind, amount_cents, account_id, category_id, note, "
        "day_of_month, enabled, last_generated_month, created_at) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(rule.kind == TransactionKind::Income ? QStringLiteral("income")
                                                            : QStringLiteral("expense"));
    query.addBindValue(rule.amount.cents());
    query.addBindValue(rule.accountId);
    query.addBindValue(rule.categoryId.has_value() ? QVariant(*rule.categoryId) : QVariant());
    query.addBindValue(rule.note);
    query.addBindValue(rule.dayOfMonth);
    query.addBindValue(rule.enabled ? 1 : 0);
    query.addBindValue(rule.lastGeneratedMonth.isEmpty() ? QVariant()
                                                         : QVariant(rule.lastGeneratedMonth));
    query.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss")));

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    return query.lastInsertId().toLongLong();
}

bool RecurringRepository::update(const RecurringRule &rule, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE recurring_rules SET kind = ?, amount_cents = ?, account_id = ?, category_id = ?, "
        "note = ?, day_of_month = ?, enabled = ? WHERE id = ?"));
    query.addBindValue(rule.kind == TransactionKind::Income ? QStringLiteral("income")
                                                            : QStringLiteral("expense"));
    query.addBindValue(rule.amount.cents());
    query.addBindValue(rule.accountId);
    query.addBindValue(rule.categoryId.has_value() ? QVariant(*rule.categoryId) : QVariant());
    query.addBindValue(rule.note);
    query.addBindValue(rule.dayOfMonth);
    query.addBindValue(rule.enabled ? 1 : 0);
    query.addBindValue(rule.id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

bool RecurringRepository::remove(qint64 id, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("DELETE FROM recurring_rules WHERE id = ?"));
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

bool RecurringRepository::setEnabled(qint64 id, bool enabled, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE recurring_rules SET enabled = ? WHERE id = ?"));
    query.addBindValue(enabled ? 1 : 0);
    query.addBindValue(id);
    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return true;
}

int RecurringRepository::generateForMonth(TransactionRepository &transactions, int year, int month,
                                          QString *error)
{
    const QString key = monthKey(year, month);
    int generated = 0;

    for (const RecurringRule &rule : listAll(error)) {
        if (!rule.enabled || rule.lastGeneratedMonth == key)
            continue;

        const QDate date(year, month, qMin(rule.dayOfMonth, QDate(year, month, 1).daysInMonth()));
        if (!date.isValid())
            continue;

        Transaction transaction;
        transaction.kind = rule.kind;
        transaction.amount = rule.amount;
        transaction.accountId = rule.accountId;
        transaction.categoryId = rule.categoryId;
        transaction.occurredAt = QDateTime(date, QTime(9, 0));
        transaction.note = rule.note;
        transaction.externalId = QStringLiteral("recurring:%1:%2").arg(rule.id).arg(key);

        if (transactions.hasExternalId(transaction.externalId))
            continue;

        if (!transactions.insert(transaction, error).has_value())
            continue;
        ++generated;

        QSqlQuery mark(m_db);
        mark.prepare(QStringLiteral("UPDATE recurring_rules SET last_generated_month = ? WHERE id = ?"));
        mark.addBindValue(key);
        mark.addBindValue(rule.id);
        mark.exec();
    }
    return generated;
}

}  // namespace keep