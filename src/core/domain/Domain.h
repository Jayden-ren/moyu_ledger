#pragma once

#include "core/domain/Money.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <optional>

namespace keep {

// ===== 枚举（与数据库 CHECK 约束里的字符串一一对应）=====

enum class AccountType { Savings, Checking, Credit, EWallet, Cash, Investment, Receivable, Payable,
                         Wealth, Stock };
enum class CategoryKind { Expense, Income };
enum class TransactionKind { Expense, Income, Transfer };

inline QString toDbString(AccountType type)
{
    switch (type) {
    case AccountType::Savings:    return QStringLiteral("savings");
    case AccountType::Checking:   return QStringLiteral("checking");
    case AccountType::Credit:     return QStringLiteral("credit");
    case AccountType::EWallet:    return QStringLiteral("ewallet");
    case AccountType::Cash:       return QStringLiteral("cash");
    case AccountType::Investment: return QStringLiteral("investment");
    case AccountType::Receivable: return QStringLiteral("receivable");
    case AccountType::Payable:    return QStringLiteral("payable");
    case AccountType::Wealth:     return QStringLiteral("wealth");
    case AccountType::Stock:      return QStringLiteral("stock");
    }
    return QStringLiteral("savings");
}

inline std::optional<AccountType> accountTypeFromDb(const QString &text)
{
    if (text == QLatin1String("savings"))    return AccountType::Savings;
    if (text == QLatin1String("checking"))   return AccountType::Checking;
    if (text == QLatin1String("credit"))     return AccountType::Credit;
    if (text == QLatin1String("ewallet"))    return AccountType::EWallet;
    if (text == QLatin1String("cash"))       return AccountType::Cash;
    if (text == QLatin1String("investment")) return AccountType::Investment;
    if (text == QLatin1String("receivable")) return AccountType::Receivable;
    if (text == QLatin1String("payable"))    return AccountType::Payable;
    if (text == QLatin1String("wealth"))     return AccountType::Wealth;
    if (text == QLatin1String("stock"))      return AccountType::Stock;
    return std::nullopt;
}

inline QString displayName(AccountType type)
{
    switch (type) {
    case AccountType::Savings:    return QStringLiteral("储蓄卡");
    case AccountType::Checking:   return QStringLiteral("活期账户");
    case AccountType::Credit:     return QStringLiteral("信用卡");
    case AccountType::EWallet:    return QStringLiteral("电子钱包");
    case AccountType::Cash:       return QStringLiteral("现金");
    case AccountType::Investment: return QStringLiteral("投资");
    case AccountType::Receivable: return QStringLiteral("应收（借出）");
    case AccountType::Payable:    return QStringLiteral("应付（欠款）");
    case AccountType::Wealth:     return QStringLiteral("理财");
    case AccountType::Stock:      return QStringLiteral("股票");
    }
    return QString();
}

// 投资类账户（支持每日收益/市值记录）
inline bool isInvestmentAccountType(AccountType type)
{
    return type == AccountType::Investment || type == AccountType::Wealth
           || type == AccountType::Stock;
}

inline QString toDbString(CategoryKind kind)
{
    return kind == CategoryKind::Income ? QStringLiteral("income") : QStringLiteral("expense");
}

inline std::optional<CategoryKind> categoryKindFromDb(const QString &text)
{
    if (text == QLatin1String("expense")) return CategoryKind::Expense;
    if (text == QLatin1String("income"))  return CategoryKind::Income;
    return std::nullopt;
}

inline QString displayName(CategoryKind kind)
{
    return kind == CategoryKind::Income ? QStringLiteral("收入") : QStringLiteral("支出");
}

inline QString toDbString(TransactionKind kind)
{
    switch (kind) {
    case TransactionKind::Expense:  return QStringLiteral("expense");
    case TransactionKind::Income:   return QStringLiteral("income");
    case TransactionKind::Transfer: return QStringLiteral("transfer");
    }
    return QStringLiteral("expense");
}

inline std::optional<TransactionKind> transactionKindFromDb(const QString &text)
{
    if (text == QLatin1String("expense"))  return TransactionKind::Expense;
    if (text == QLatin1String("income"))   return TransactionKind::Income;
    if (text == QLatin1String("transfer")) return TransactionKind::Transfer;
    return std::nullopt;
}

inline QString displayName(TransactionKind kind)
{
    switch (kind) {
    case TransactionKind::Expense:  return QStringLiteral("支出");
    case TransactionKind::Income:   return QStringLiteral("收入");
    case TransactionKind::Transfer: return QStringLiteral("转账");
    }
    return QString();
}

// 收入为正、支出/转账出账为负：给余额与 KPI 用的统一符号
inline qint64 signedCents(TransactionKind kind, const Money &amount)
{
    return kind == TransactionKind::Income ? amount.cents() : -amount.cents();
}

// ===== 领域对象 =====

struct Account
{
    qint64 id = 0;
    QString name;
    AccountType type = AccountType::Savings;
    QString currency = QStringLiteral("CNY");
    Money openingBalance;
    std::optional<qint64> creditLimitCents;
    QString icon;
    QString color;
    int sortOrder = 0;
    bool archived = false;
    std::optional<qint64> ledgerId;  // 所属账本（V3 多账本）
};

struct Category
{
    qint64 id = 0;
    std::optional<qint64> parentId;
    QString name;
    CategoryKind kind = CategoryKind::Expense;
    QString icon;
    QString color;
    int sortOrder = 0;
    bool builtin = false;
    bool archived = false;
};

struct Transaction
{
    qint64 id = 0;
    TransactionKind kind = TransactionKind::Expense;
    Money amount;
    qint64 accountId = 0;
    std::optional<qint64> toAccountId;
    std::optional<qint64> categoryId;
    QDateTime occurredAt;
    QString note;
    QString payee;
    QString externalId;  // 账单导入去重键（交易单号或内容哈希）
};

// ===== 报表用聚合结构 =====

struct MonthSummary
{
    Money income;
    Money expense;
    int count = 0;

    [[nodiscard]] Money balance() const { return income - expense; }
};

struct CategoryTotal
{
    qint64 categoryId = 0;
    QString name;
    QString color;
    Money total;
    double share = 0.0;  // 0.0 ~ 1.0
};

struct MonthPoint
{
    int year = 0;
    int month = 0;
    Money income;
    Money expense;
};

// 流水过滤条件：关键字（摘要/收款方/金额）+ 账户 + 分类
struct TransactionFilter
{
    QString keyword;
    std::optional<qint64> accountId;
    std::optional<qint64> categoryId;

    [[nodiscard]] bool isEmpty() const
    {
        return keyword.trimmed().isEmpty() && !accountId.has_value() && !categoryId.has_value();
    }
};

// 单账户在某月的收支汇总（报表「账户维度」用）
struct AccountTotal
{
    qint64 accountId = 0;
    QString name;
    QString color;
    Money income;
    Money expense;
    int count = 0;
};

struct AccountWithBalance
{
    Account account;
    Money balance;
};

}  // namespace keep