#pragma once

#include "core/domain/Domain.h"
#include "core/domain/Money.h"

#include <QDate>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

#include <optional>

namespace keep {

// 投资类账户的「每日收益」记录（理财 / 股票 / 投资）
struct AccountDailyRecord
{
    qint64 id = 0;
    qint64 accountId = 0;
    QDate date;
    Money income;                 // 当日收益（可负）
    Money commission;             // 当日佣金 / 手续费
    std::optional<Money> marketValue;  // 当日收盘市值（可选）
    QString note;
    qint64 incomeTransactionId = 0;     // 联动生成的收入流水
    qint64 commissionTransactionId = 0; // 联动生成的佣金支出流水
};

class AccountDailyRecordRepository
{
public:
    explicit AccountDailyRecordRepository(QSqlDatabase db);

    [[nodiscard]] std::optional<AccountDailyRecord> findByAccountAndDate(
        qint64 accountId, const QDate &date, QString *error = nullptr) const;
    [[nodiscard]] std::optional<AccountDailyRecord> findById(qint64 id, QString *error = nullptr) const;
    [[nodiscard]] QVector<AccountDailyRecord> listByAccount(qint64 accountId, int limit = 60,
                                                            QString *error = nullptr) const;
    // 指定日期之前最近的一条记录（用于「市值推算收益」）
    [[nodiscard]] std::optional<AccountDailyRecord> latestBefore(qint64 accountId, const QDate &date,
                                                                 QString *error = nullptr) const;
    [[nodiscard]] Money totalIncome(qint64 accountId, QString *error = nullptr) const;
    [[nodiscard]] Money totalCommission(qint64 accountId, QString *error = nullptr) const;

    [[nodiscard]] std::optional<qint64> insert(const AccountDailyRecord &record, QString *error = nullptr);
    [[nodiscard]] bool update(const AccountDailyRecord &record, QString *error = nullptr);
    [[nodiscard]] bool remove(qint64 id, QString *error = nullptr);

private:
    QSqlDatabase m_db;
};

}  // namespace keep