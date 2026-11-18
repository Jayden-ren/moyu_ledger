#include "core/data/BudgetRepository.h"

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

}  // namespace

BudgetRepository::BudgetRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

std::optional<Money> BudgetRepository::monthlyLimit(const QString &yearMonth, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT amount_cents FROM budgets WHERE year_month = ? AND category_id IS NULL LIMIT 1"));
    query.addBindValue(yearMonth);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;

    return Money::fromCents(query.value(0).toLongLong());
}

bool BudgetRepository::setMonthlyLimit(const QString &yearMonth, const Money &amount, QString *error)
{
    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    QSqlQuery remove(m_db);
    remove.prepare(QStringLiteral("DELETE FROM budgets WHERE year_month = ? AND category_id IS NULL"));
    remove.addBindValue(yearMonth);
    if (!remove.exec()) {
        setError(error, remove);
        m_db.rollback();
        return false;
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO budgets(year_month, category_id, amount_cents, rollover) VALUES (?, NULL, ?, 0)"));
    insert.addBindValue(yearMonth);
    insert.addBindValue(amount.cents());
    if (!insert.exec()) {
        setError(error, insert);
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

QVector<BudgetRepository::CategoryBudget> BudgetRepository::categoryBudgets(
    const QString &yearMonth, QString *error) const
{
    QVector<CategoryBudget> rows;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT category_id, amount_cents FROM budgets "
        "WHERE year_month = ? AND category_id IS NOT NULL ORDER BY category_id"));
    query.addBindValue(yearMonth);

    if (!query.exec()) {
        setError(error, query);
        return rows;
    }

    while (query.next()) {
        CategoryBudget row;
        row.categoryId = query.value(0).toLongLong();
        row.amount = Money::fromCents(query.value(1).toLongLong());
        rows.append(row);
    }
    return rows;
}

bool BudgetRepository::setCategoryBudget(const QString &yearMonth, qint64 categoryId,
                                         const Money &amount, QString *error)
{
    if (categoryId <= 0) {
        if (error)
            *error = QStringLiteral("分类无效");
        return false;
    }

    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    QSqlQuery remove(m_db);
    remove.prepare(QStringLiteral(
        "DELETE FROM budgets WHERE year_month = ? AND category_id = ?"));
    remove.addBindValue(yearMonth);
    remove.addBindValue(categoryId);
    if (!remove.exec()) {
        setError(error, remove);
        m_db.rollback();
        return false;
    }

    // amount 为 0 只做清除
    if (amount.cents() <= 0) {
        if (!m_db.commit()) {
            if (error)
                *error = m_db.lastError().text();
            return false;
        }
        return true;
    }

    QSqlQuery insert(m_db);
    insert.prepare(QStringLiteral(
        "INSERT INTO budgets(year_month, category_id, amount_cents, rollover) VALUES (?, ?, ?, 0)"));
    insert.addBindValue(yearMonth);
    insert.addBindValue(categoryId);
    insert.addBindValue(amount.cents());
    if (!insert.exec()) {
        setError(error, insert);
        m_db.rollback();
        return false;
    }

    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

}  // namespace keep