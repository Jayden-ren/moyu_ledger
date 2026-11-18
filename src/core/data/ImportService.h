#pragma once

#include "core/domain/Domain.h"

#include <QDateTime>
#include <QString>
#include <QVector>

namespace keep {

class TransactionRepository;

struct ImportRow
{
    QDateTime occurredAt;
    TransactionKind kind = TransactionKind::Expense;
    Money amount;           // 恒为正数
    QString note;           // 商品 / 摘要
    QString payee;          // 交易对方
    QString externalId;     // 交易单号（去重键）
    QString tradeType;      // 交易类型（微信/支付宝的「交易类型」列）
    QString merchantNo;     // 商户单号
    QString counterpartyAccount;  // 对方账号
};

struct ImportPreview
{
    bool ok = false;
    QString message;
    QString source;         // wechat / alipay / generic
    QVector<ImportRow> rows;

    [[nodiscard]] int expenseCount() const;
    [[nodiscard]] int incomeCount() const;
    [[nodiscard]] Money expenseTotal() const;
    [[nodiscard]] Money incomeTotal() const;
};

struct ImportOutcome
{
    int imported = 0;
    int skipped = 0;        // 重复或状态无效
};

// 账单导入：自动识别支付宝 / 微信导出的 CSV（含编码嗅探），按交易单号去重
class ImportService
{
public:
    [[nodiscard]] static ImportPreview preview(const QString &path, QString *error = nullptr);

    [[nodiscard]] static ImportOutcome import(TransactionRepository &repository,
                                              const ImportPreview &preview,
                                              qint64 accountId,
                                              std::optional<qint64> categoryId,
                                              QString *error = nullptr);

private:
    [[nodiscard]] static QString readText(const QByteArray &bytes);
    [[nodiscard]] static QVector<QStringList> parseCsv(const QString &text, QChar delimiter);
    [[nodiscard]] static QDateTime parseDateTime(const QString &text);
    [[nodiscard]] static std::optional<Money> parseAmount(const QString &text);
};

}  // namespace keep