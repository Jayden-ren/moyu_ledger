#pragma once

#include "core/domain/Domain.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

namespace keep {

class CategoryRepository
{
public:
    explicit CategoryRepository(QSqlDatabase db);

    [[nodiscard]] QVector<Category> listAll(bool includeArchived = false, QString *error = nullptr) const;
    [[nodiscard]] QVector<Category> listByKind(CategoryKind kind, bool includeArchived = false,
                                              QString *error = nullptr) const;
    [[nodiscard]] std::optional<Category> findById(qint64 id, QString *error = nullptr) const;
    [[nodiscard]] std::optional<Category> findByName(const QString &name, CategoryKind kind,
                                                     QString *error = nullptr) const;
    [[nodiscard]] int count(QString *error = nullptr) const;

    [[nodiscard]] std::optional<qint64> insert(const Category &category, QString *error = nullptr);
    [[nodiscard]] bool update(const Category &category, QString *error = nullptr);
    [[nodiscard]] bool setArchived(qint64 id, bool archived, QString *error = nullptr);

    // 首次运行写入内置分类；库里已有分类则直接返回 true（幂等）
    [[nodiscard]] bool seedBuiltins(QString *error = nullptr);

    // 为大类播种内置小类（幂等：已有任何小类就跳过）
    [[nodiscard]] bool seedSubcategories(QString *error = nullptr);

    // 投资类账户联动用的分类（投资收益 / 投资亏损 / 手续费），幂等
    [[nodiscard]] bool seedInvestmentCategories(QString *error = nullptr);

private:
    QSqlDatabase m_db;
};

}  // namespace keep