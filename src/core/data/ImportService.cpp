#include "core/data/ImportService.h"

#include "core/data/TransactionRepository.h"

#include <QCryptographicHash>
#include <QFile>
#include <QRegularExpression>
#include <QStringConverter>
#include <QStringDecoder>

namespace keep {

namespace {

// 在表头里找列：返回匹配到的列索引，找不到 -1
int findColumn(const QStringList &header, const QStringList &needles, int skipIndex = -1)
{
    for (int i = 0; i < header.size(); ++i) {
        if (i == skipIndex)
            continue;
        const QString cell = header.at(i).trimmed();
        for (const QString &needle : needles) {
            if (cell.contains(needle, Qt::CaseInsensitive))
                return i;
        }
    }
    return -1;
}

QString cellAt(const QStringList &row, int index)
{
    if (index < 0 || index >= row.size())
        return QString();
    return row.at(index).trimmed();
}

}  // namespace

int ImportPreview::expenseCount() const
{
    int count = 0;
    for (const ImportRow &row : rows) {
        if (row.kind == TransactionKind::Expense)
            ++count;
    }
    return count;
}

int ImportPreview::incomeCount() const
{
    return rows.size() - expenseCount();
}

Money ImportPreview::expenseTotal() const
{
    Money total;
    for (const ImportRow &row : rows) {
        if (row.kind == TransactionKind::Expense)
            total += row.amount;
    }
    return total;
}

Money ImportPreview::incomeTotal() const
{
    Money total;
    for (const ImportRow &row : rows) {
        if (row.kind == TransactionKind::Income)
            total += row.amount;
    }
    return total;
}

QString ImportService::readText(const QByteArray &bytes)
{
    // 1) UTF-8 BOM
    if (bytes.startsWith("\xEF\xBB\xBF")) {
        return QStringDecoder(QStringConverter::Utf8).decode(bytes.mid(3));
    }

    // 2) 先按 UTF-8 严格解析，出现替换字符则回退到系统编码（中文 Windows 为 GBK）
    QStringDecoder utf8(QStringConverter::Utf8);
    const QString decoded = utf8.decode(bytes);
    if (!utf8.hasError() && !decoded.contains(QChar(0xFFFD)))
        return decoded;

    return QStringDecoder(QStringConverter::System).decode(bytes);
}

QVector<QStringList> ImportService::parseCsv(const QString &text, QChar delimiter)
{
    QVector<QStringList> rows;
    QStringList current;
    QString field;
    bool inQuotes = false;

    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);

        if (inQuotes) {
            if (ch == QLatin1Char('"')) {
                if (i + 1 < text.size() && text.at(i + 1) == QLatin1Char('"')) {
                    field += QLatin1Char('"');
                    ++i;
                } else {
                    inQuotes = false;
                }
            } else {
                field += ch;
            }
            continue;
        }

        if (ch == QLatin1Char('"')) {
            inQuotes = true;
        } else if (ch == delimiter) {
            current.append(field);
            field.clear();
        } else if (ch == QLatin1Char('\n')) {
            current.append(field);
            field.clear();
            if (!current.isEmpty() && !(current.size() == 1 && current.first().trimmed().isEmpty()))
                rows.append(current);
            current.clear();
        } else if (ch != QLatin1Char('\r')) {
            field += ch;
        }
    }

    if (!field.isEmpty() || !current.isEmpty()) {
        current.append(field);
        rows.append(current);
    }
    return rows;
}

QDateTime ImportService::parseDateTime(const QString &text)
{
    const QString value = text.trimmed();
    static const QStringList formats{
        QStringLiteral("yyyy-MM-dd HH:mm:ss"), QStringLiteral("yyyy-MM-dd HH:mm"),
        QStringLiteral("yyyy/M/d H:mm:ss"),    QStringLiteral("yyyy/M/d H:mm"),
        QStringLiteral("yyyy-MM-dd"),          QStringLiteral("yyyy/M/d"),
    };
    for (const QString &format : formats) {
        const QDateTime parsed = QDateTime::fromString(value, format);
        if (parsed.isValid())
            return parsed;
    }
    return {};
}

std::optional<Money> ImportService::parseAmount(const QString &text)
{
    QString value = text.trimmed();
    value.remove(QChar(0x00A5));
    value.remove(QChar(0xFFE5));
    value.remove(QLatin1Char(','));
    value.remove(QLatin1Char(' '));
    if (value.isEmpty())
        return std::nullopt;
    return Money::fromYuan(value);
}

ImportPreview ImportService::preview(const QString &path, QString *error)
{
    ImportPreview result;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        result.message = QStringLiteral("无法打开文件：%1").arg(file.errorString());
        if (error)
            *error = result.message;
        return result;
    }
    const QByteArray bytes = file.readAll();
    file.close();

    const QString text = readText(bytes);
    QChar delimiter = QLatin1Char(',');
    if (text.count(QLatin1Char('\t')) > text.count(QLatin1Char(',')))
        delimiter = QLatin1Char('\t');

    const QVector<QStringList> rows = parseCsv(text, delimiter);
    if (rows.isEmpty()) {
        result.message = QStringLiteral("文件为空或格式无法解析");
        return result;
    }

    // 找到表头行（包含「交易时间」或「金额」）
    int headerIndex = -1;
    for (int i = 0; i < rows.size() && i < 40; ++i) {
        const QString joined = rows.at(i).join(QLatin1Char(','));
        if (joined.contains(QStringLiteral("交易时间")) || joined.contains(QStringLiteral("金额"))) {
            headerIndex = i;
            break;
        }
    }
    if (headerIndex < 0) {
        result.message = QStringLiteral("未识别到表头，请使用支付宝 / 微信导出的 CSV 账单");
        return result;
    }

    const QStringList header = rows.at(headerIndex);

    const int dateCol = findColumn(header, {QStringLiteral("交易时间"), QStringLiteral("时间")});
    const int amountCol = findColumn(header, {QStringLiteral("金额")});
    const int directionCol = findColumn(header,
                                        {QStringLiteral("收/支"), QStringLiteral("收支"),
                                         QStringLiteral("借贷标志")});
    const int payeeCol = findColumn(header, {QStringLiteral("交易对方"), QStringLiteral("对方")});
    const int noteCol = findColumn(header,
                                   {QStringLiteral("商品说明"), QStringLiteral("商品"),
                                    QStringLiteral("备注")});
    const int statusCol = findColumn(header, {QStringLiteral("交易状态"), QStringLiteral("当前状态")});
    const int idCol = findColumn(header, {QStringLiteral("交易单号"), QStringLiteral("交易订单号")});
    const int tradeTypeCol = findColumn(header, {QStringLiteral("交易类型")});
    const int merchantNoCol = findColumn(header, {QStringLiteral("商户单号"), QStringLiteral("商家订单号")});
    const int counterpartyCol = findColumn(header, {QStringLiteral("对方账号"), QStringLiteral("对方帐号")});

    if (dateCol < 0 || amountCol < 0) {
        result.message = QStringLiteral("缺少「交易时间」或「金额」列，无法导入");
        return result;
    }

    result.source = header.join(QLatin1Char(','))
                            .contains(QStringLiteral("收/支"))
                        ? QStringLiteral("wechat")
                        : QStringLiteral("alipay");

    for (int i = headerIndex + 1; i < rows.size(); ++i) {
        const QStringList &row = rows.at(i);

        const QString status = cellAt(row, statusCol);
        if (status.contains(QStringLiteral("失败")) || status.contains(QStringLiteral("关闭"))
            || status.contains(QStringLiteral("已退款")) || status.contains(QStringLiteral("全额退款"))) {
            continue;
        }

        QString direction = cellAt(row, directionCol);
        if (direction.contains(QStringLiteral("不计收支")))
            continue;

        const QString tradeType = cellAt(row, tradeTypeCol);
        if (direction.isEmpty() && !tradeType.isEmpty()) {
            if (tradeType.contains(QStringLiteral("收入")))
                direction = QStringLiteral("收入");
            else if (tradeType.contains(QStringLiteral("支出")))
                direction = QStringLiteral("支出");
        }

        const auto amount = parseAmount(cellAt(row, amountCol));
        if (!amount.has_value() || amount->isZero())
            continue;

        const QDateTime occurredAt = parseDateTime(cellAt(row, dateCol));
        if (!occurredAt.isValid())
            continue;

        ImportRow imported;
        imported.occurredAt = occurredAt;
        imported.tradeType = tradeType;
        imported.merchantNo = cellAt(row, merchantNoCol);
        imported.counterpartyAccount = cellAt(row, counterpartyCol);
        imported.payee = cellAt(row, payeeCol);
        if (imported.payee.isEmpty())
            imported.payee = imported.counterpartyAccount;
        imported.note = cellAt(row, noteCol);
        if (imported.note.isEmpty())
            imported.note = imported.payee;
        if (imported.note.isEmpty())
            imported.note = imported.tradeType;

        const bool negative = amount->isNegative();
        const bool incomeByDirection = direction.contains(QStringLiteral("收入"))
                                       || direction.contains(QStringLiteral("收"));
        imported.kind = (!negative && incomeByDirection) ? TransactionKind::Income
                                                         : TransactionKind::Expense;
        imported.amount = Money(negative ? -amount->cents() : amount->cents());

        QString externalId = cellAt(row, idCol);
        if (externalId.isEmpty())
            externalId = imported.merchantNo;
        if (externalId.isEmpty()) {
            const QByteArray hash = QCryptographicHash::hash(
                QStringLiteral("%1|%2|%3|%4")
                    .arg(imported.occurredAt.toString(Qt::ISODate))
                    .arg(imported.amount.cents())
                    .arg(imported.note)
                    .arg(imported.payee)
                    .toUtf8(),
                QCryptographicHash::Sha1);
            externalId = QString::fromLatin1(hash.toHex().left(20));
        }
        imported.externalId = externalId;

        result.rows.append(imported);
    }

    if (result.rows.isEmpty()) {
        result.message = QStringLiteral("没有可导入的有效流水（可能都被过滤）");
        return result;
    }

    result.ok = true;
    result.message = QStringLiteral("识别到 %1 笔流水").arg(result.rows.size());
    return result;
}

ImportOutcome ImportService::import(TransactionRepository &repository,
                                    const ImportPreview &preview,
                                    qint64 accountId,
                                    std::optional<qint64> categoryId,
                                    QString *error)
{
    ImportOutcome outcome;
    if (accountId <= 0) {
        if (error)
            *error = QStringLiteral("请选择入账账户");
        return outcome;
    }

    for (const ImportRow &row : preview.rows) {
        if (repository.hasExternalId(row.externalId)) {
            ++outcome.skipped;
            continue;
        }

        Transaction transaction;
        transaction.kind = row.kind;
        transaction.amount = row.amount;
        transaction.accountId = accountId;
        transaction.categoryId = categoryId;
        transaction.occurredAt = row.occurredAt;
        transaction.note = row.note;
        transaction.payee = row.payee;
        transaction.externalId = row.externalId;

        if (!repository.insert(transaction, error).has_value()) {
            // 单条失败按跳过处理，继续导入其余
            ++outcome.skipped;
            if (error)
                error->clear();
            continue;
        }
        ++outcome.imported;
    }
    return outcome;
}

}  // namespace keep