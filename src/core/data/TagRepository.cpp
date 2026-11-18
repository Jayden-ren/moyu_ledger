#include "core/data/TagRepository.h"

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

QString monthStartText(int year, int month)
{
    return QStringLiteral("%1-%2-01T00:00:00")
        .arg(year)
        .arg(month, 2, 10, QLatin1Char('0'));
}

QString monthEndText(int year, int month)
{
    const QDate first(year, month, 1);
    const QDate next = first.addMonths(1);
    return QStringLiteral("%1-%2-01T00:00:00")
        .arg(next.year())
        .arg(next.month(), 2, 10, QLatin1Char('0'));
}

}  // namespace

TagRepository::TagRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QStringList TagRepository::tagsFor(qint64 transactionId, QString *error) const
{
    QStringList names;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT t.name FROM transaction_tags tt JOIN tags t ON t.id = tt.tag_id "
        "WHERE tt.transaction_id = ? ORDER BY t.name COLLATE NOCASE"));
    query.addBindValue(transactionId);
    if (!query.exec()) {
        setError(error, query);
        return names;
    }
    while (query.next())
        names.append(query.value(0).toString());
    return names;
}

QHash<qint64, QStringList> TagRepository::tagsForTransactions(const QVector<qint64> &ids,
                                                             QString *error) const
{
    QHash<qint64, QStringList> result;
    if (ids.isEmpty())
        return result;

    QStringList placeholders;
    placeholders.reserve(ids.size());
    for (int i = 0; i < ids.size(); ++i)
        placeholders.append(QStringLiteral("?"));

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT tt.transaction_id, t.name FROM transaction_tags tt "
                                 "JOIN tags t ON t.id = tt.tag_id WHERE tt.transaction_id IN (%1) "
                                 "ORDER BY t.name COLLATE NOCASE")
                      .arg(placeholders.join(QLatin1Char(','))));
    for (qint64 id : ids)
        query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return result;
    }
    while (query.next())
        result[query.value(0).toLongLong()].append(query.value(1).toString());
    return result;
}

bool TagRepository::setTags(qint64 transactionId, const QStringList &names, QString *error)
{
    if (transactionId <= 0) {
        if (error)
            *error = QStringLiteral("流水无效");
        return false;
    }

    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    QSqlQuery remove(m_db);
    remove.prepare(QStringLiteral("DELETE FROM transaction_tags WHERE transaction_id = ?"));
    remove.addBindValue(transactionId);
    if (!remove.exec()) {
        setError(error, remove);
        m_db.rollback();
        return false;
    }

    QStringList seen;
    for (const QString &raw : names) {
        const QString name = raw.trimmed();
        if (name.isEmpty() || seen.contains(name, Qt::CaseInsensitive))
            continue;
        seen.append(name);

        QSqlQuery create(m_db);
        create.prepare(QStringLiteral("INSERT OR IGNORE INTO tags(name) VALUES (?)"));
        create.addBindValue(name);
        if (!create.exec()) {
            setError(error, create);
            m_db.rollback();
            return false;
        }

        QSqlQuery lookup(m_db);
        lookup.prepare(QStringLiteral("SELECT id FROM tags WHERE name = ? COLLATE NOCASE"));
        lookup.addBindValue(name);
        if (!lookup.exec() || !lookup.next()) {
            if (error)
                *error = QStringLiteral("标签写入失败：%1").arg(name);
            m_db.rollback();
            return false;
        }
        const qint64 tagId = lookup.value(0).toLongLong();

        QSqlQuery link(m_db);
        link.prepare(QStringLiteral(
            "INSERT OR IGNORE INTO transaction_tags(transaction_id, tag_id) VALUES (?, ?)"));
        link.addBindValue(transactionId);
        link.addBindValue(tagId);
        if (!link.exec()) {
            setError(error, link);
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    return true;
}

QVector<TagTotal> TagRepository::tagTotals(int year, int month, QString *error) const
{
    QVector<TagTotal> totals;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT t.name, COALESCE(SUM(x.amount_cents), 0), COUNT(DISTINCT x.id) "
        "FROM tags t "
        "JOIN transaction_tags tt ON tt.tag_id = t.id "
        "JOIN transactions x ON x.id = tt.transaction_id "
        "WHERE x.kind = 'expense' AND x.occurred_at >= ? AND x.occurred_at < ?%1 "
        "GROUP BY t.id ORDER BY 2 DESC")
            .arg(m_ledgerId.has_value()
                     ? QStringLiteral(" AND x.account_id IN (SELECT id FROM accounts WHERE ledger_id = ?)")
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
        TagTotal row;
        row.name = query.value(0).toString();
        row.total = Money::fromCents(query.value(1).toLongLong());
        row.count = query.value(2).toInt();
        totals.append(row);
    }
    return totals;
}

QStringList TagRepository::allTagNames(QString *error) const
{
    QStringList names;
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT name FROM tags ORDER BY name COLLATE NOCASE"))) {
        setError(error, query);
        return names;
    }
    while (query.next())
        names.append(query.value(0).toString());
    return names;
}

}  // namespace keep