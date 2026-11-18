#pragma once

#include "core/domain/Domain.h"

#include <QHash>
#include <QSqlDatabase>
#include <QStringList>
#include <QVector>

namespace keep {

// 标签在某月的消费汇总（报表「标签维度」用）
struct TagTotal
{
    QString name;
    Money total;
    int count = 0;
};

// 标签仓储：tags / transaction_tags 两张表的读写
class TagRepository
{
public:
    explicit TagRepository(QSqlDatabase db);

    // 单笔/批量取标签名
    [[nodiscard]] QStringList tagsFor(qint64 transactionId, QString *error = nullptr) const;
    [[nodiscard]] QHash<qint64, QStringList> tagsForTransactions(const QVector<qint64> &ids,
                                                                QString *error = nullptr) const;

    // 覆盖式写入（先删后插，标签不存在则创建）；names 里空项会被忽略
    [[nodiscard]] bool setTags(qint64 transactionId, const QStringList &names,
                               QString *error = nullptr);

    // 某月按标签聚合支出（只统计支出流水）
    [[nodiscard]] QVector<TagTotal> tagTotals(int year, int month, QString *error = nullptr) const;

    [[nodiscard]] QStringList allTagNames(QString *error = nullptr) const;

    // 账本作用域：标签统计只算该账本账户下的流水
    void setLedgerScope(std::optional<qint64> ledgerId) { m_ledgerId = ledgerId; }

private:
    QSqlDatabase m_db;
    std::optional<qint64> m_ledgerId;
};

}  // namespace keep