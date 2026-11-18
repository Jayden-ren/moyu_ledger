#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QVector>

namespace keep {

// 账本（个人 / 家庭 / 育婴 / 恋爱 …）：账户挂在账本下，流水通过账户归属账本
struct Ledger
{
    qint64 id = 0;
    QString name;
    QString color;
    bool isDefault = false;
    int sortOrder = 0;
    int accountCount = 0;
};

class LedgerRepository
{
public:
    explicit LedgerRepository(QSqlDatabase db);

    [[nodiscard]] QVector<Ledger> listAll(QString *error = nullptr) const;
    [[nodiscard]] std::optional<qint64> insert(const QString &name, const QString &color,
                                               QString *error = nullptr);
    [[nodiscard]] bool rename(qint64 id, const QString &name, QString *error = nullptr);
    // 只允许删除非默认、且没有账户的账本
    [[nodiscard]] bool remove(qint64 id, QString *error = nullptr);
    [[nodiscard]] std::optional<qint64> defaultId(QString *error = nullptr) const;

private:
    QSqlDatabase m_db;
};

}  // namespace keep