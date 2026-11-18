#pragma once

#include "core/domain/Domain.h"

#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

namespace keep {

class TransactionRepository
{
public:
    explicit TransactionRepository(QSqlDatabase db);

    [[nodiscard]] std::optional<qint64> insert(const Transaction &transaction, QString *error = nullptr);
    [[nodiscard]] bool update(const Transaction &transaction, QString *error = nullptr);
    [[nodiscard]] bool remove(qint64 id, QString *error = nullptr);
    [[nodiscard]] std::optional<Transaction> findById(qint64 id, QString *error = nullptr) const;

    // 按账户聚合某月收支（含转账，转账只算「账户流出」不计入支出总额口径由调用方决定）
    [[nodiscard]] QVector<AccountTotal> accountTotals(int year, int month,
                                                     QString *error = nullptr) const;

    [[nodiscard]] QVector<Transaction> listByMonth(int year, int month, QString *error = nullptr) const;
    [[nodiscard]] QVector<Transaction> listByMonth(int year, int month,
                                                  const TransactionFilter &filter,
                                                  QString *error = nullptr) const;
    [[nodiscard]] MonthSummary monthSummary(int year, int month, QString *error = nullptr) const;

    // 分类占比（默认支出），按金额降序，share 已归一到 0..1
    [[nodiscard]] QVector<CategoryTotal> categoryShares(int year, int month,
                                                       TransactionKind kind = TransactionKind::Expense,
                                                       QString *error = nullptr) const;

    // 以 endYear/endMonth 结尾、连续 monthCount 个月的收支
    [[nodiscard]] QVector<MonthPoint> monthlyTrend(int endYear, int endMonth, int monthCount = 6,
                                                   QString *error = nullptr) const;

    // 账本作用域：设置后所有聚合/列表只统计该账本账户下的流水
    void setLedgerScope(std::optional<qint64> ledgerId) { m_ledgerId = ledgerId; }
    [[nodiscard]] std::optional<qint64> ledgerScope() const { return m_ledgerId; }

    [[nodiscard]] int countAll(QString *error = nullptr) const;

    // 导入去重：按交易单号判断是否已存在
    [[nodiscard]] bool hasExternalId(const QString &externalId, QString *error = nullptr) const;

    [[nodiscard]] static QString monthStartText(int year, int month);
    [[nodiscard]] static QString monthEndText(int year, int month);  // 下月 1 号（不含）

private:
    QSqlDatabase m_db;
    std::optional<qint64> m_ledgerId;
};

}  // namespace keep