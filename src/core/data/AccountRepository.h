#pragma once

#include "core/domain/Domain.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

namespace keep {

// 账户仓储：余额一律由「期初 + 流水实时推算」，不存冗余余额字段。
class AccountRepository
{
public:
    explicit AccountRepository(QSqlDatabase db);

    [[nodiscard]] QVector<Account> listAll(bool includeArchived = false, QString *error = nullptr) const;
    [[nodiscard]] QVector<AccountWithBalance> listWithBalances(bool includeArchived = false,
                                                               QString *error = nullptr) const;
    // 账本作用域：设置后列表/净资产只统计该账本的账户
    void setLedgerScope(std::optional<qint64> ledgerId) { m_ledgerId = ledgerId; }

    [[nodiscard]] std::optional<Account> findById(qint64 id, QString *error = nullptr) const;

    // 单账户余额（含转入转出）
    [[nodiscard]] std::optional<Money> balance(qint64 id, QString *error = nullptr) const;
    // 净资产 = 所有未归档账户余额之和（信用卡为负债，余额为负）
    [[nodiscard]] std::optional<Money> netWorth(QString *error = nullptr) const;

    [[nodiscard]] std::optional<qint64> insert(const Account &account, QString *error = nullptr);
    [[nodiscard]] bool update(const Account &account, QString *error = nullptr);
    [[nodiscard]] bool setArchived(qint64 id, bool archived, QString *error = nullptr);

private:
    QSqlDatabase m_db;
    std::optional<qint64> m_ledgerId;
};

}  // namespace keep