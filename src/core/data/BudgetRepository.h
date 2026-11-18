#pragma once

#include "core/domain/Domain.h"

#include <QSqlDatabase>
#include <QVector>
#include <QString>

#include <optional>

namespace keep {

// 预算仓储。注意：budgets 表的 UNIQUE(year_month, category_id) 对 NULL 不生效，
// 所以「总预算」这一行（category_id IS NULL）由本类保证唯一（先删后插）。
class BudgetRepository
{
public:
    explicit BudgetRepository(QSqlDatabase db);

    [[nodiscard]] std::optional<Money> monthlyLimit(const QString &yearMonth,
                                                    QString *error = nullptr) const;
    [[nodiscard]] bool setMonthlyLimit(const QString &yearMonth, const Money &amount,
                                       QString *error = nullptr);

    struct CategoryBudget
    {
        qint64 categoryId = 0;
        Money amount;
    };

    // 某月的分类预算（不含总预算）
    [[nodiscard]] QVector<CategoryBudget> categoryBudgets(const QString &yearMonth,
                                                          QString *error = nullptr) const;

    // amount 为 0 表示清除该分类预算
    [[nodiscard]] bool setCategoryBudget(const QString &yearMonth, qint64 categoryId,
                                         const Money &amount, QString *error = nullptr);

private:
    QSqlDatabase m_db;
};

}  // namespace keep