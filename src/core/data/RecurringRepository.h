#pragma once

#include "core/domain/Domain.h"

#include <QSqlDatabase>
#include <QVector>

namespace keep {

class TransactionRepository;

// 定期账单规则（房租、订阅等按月自动生成）
struct RecurringRule
{
    qint64 id = 0;
    TransactionKind kind = TransactionKind::Expense;
    Money amount;
    qint64 accountId = 0;
    std::optional<qint64> categoryId;
    QString note;
    int dayOfMonth = 1;      // 1–28，避免月末不存在
    bool enabled = true;
    QString lastGeneratedMonth;  // "YYYY-MM"
};

class RecurringRepository
{
public:
    explicit RecurringRepository(QSqlDatabase db);

    [[nodiscard]] QVector<RecurringRule> listAll(QString *error = nullptr) const;
    [[nodiscard]] std::optional<qint64> insert(const RecurringRule &rule, QString *error = nullptr);
    [[nodiscard]] bool update(const RecurringRule &rule, QString *error = nullptr);
    [[nodiscard]] bool remove(qint64 id, QString *error = nullptr);
    [[nodiscard]] bool setEnabled(qint64 id, bool enabled, QString *error = nullptr);

    // 为指定月份补生成（幂等：last_generated_month + external_id 双重判断）
    int generateForMonth(TransactionRepository &transactions, int year, int month,
                         QString *error = nullptr);

private:
    QSqlDatabase m_db;
};

}  // namespace keep