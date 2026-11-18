#include "core/data/DemoSeeder.h"

#include "core/data/AccountRepository.h"
#include "core/data/BudgetRepository.h"
#include "core/data/CategoryRepository.h"
#include "core/data/TransactionRepository.h"
#include "core/domain/Domain.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QSqlError>
#include <QSqlQuery>
#include <QString>
#include <QVariant>

namespace keep {

namespace {

struct AccountSeed
{
    const char *name;
    AccountType type;
    qint64 openingCents;  // 反推值：目标余额 − 种子流水净影响
    const char *icon;
    const char *color;
};

// 目标余额：28,460.32 / 3,218.90 / 842.15 / 560.00 / −2,184.50
constexpr AccountSeed kAccounts[] = {
    {"招商银行储蓄卡", AccountType::Savings, 4539032, "卡", "#3B82F6"},
    {"支付宝", AccountType::EWallet, 338230, "支", "#06B6D4"},
    {"微信零钱", AccountType::EWallet, 126165, "微", "#22C55E"},
    {"现金", AccountType::Cash, 56000, "现", "#64748B"},
    {"招商银行信用卡", AccountType::Credit, -61750, "信", "#EF4444"},
};

struct TransactionSeed
{
    const char *occurredAt;  // yyyy-MM-ddTHH:mm:ss（本地时间）
    TransactionKind kind;
    qint64 amountCents;
    const char *category;
    const char *account;
    const char *note;
};

// 设计基准第 4.5 节，按时间倒序；合计支出 5,907.90 / 收入 18,758.00
constexpr TransactionSeed kTransactions[] = {
    {"2026-11-11T12:24:00", TransactionKind::Expense, 2300, "餐饮", "支付宝", "午餐 · 沙县小吃"},
    {"2026-11-11T09:12:00", TransactionKind::Expense, 600, "交通", "微信零钱", "地铁 2 号线"},
    {"2026-11-11T08:40:00", TransactionKind::Expense, 26800, "购物", "招商银行信用卡", "键帽 · 轴体"},
    {"2026-11-10T15:05:00", TransactionKind::Expense, 3800, "餐饮", "支付宝", "星巴克 香草拿铁"},
    {"2026-11-10T11:30:00", TransactionKind::Expense, 129900, "购物", "招商银行信用卡", "京东 · 27 寸显示器"},
    {"2026-11-09T20:00:00", TransactionKind::Expense, 320000, "住房", "招商银行储蓄卡", "房租 · 11 月"},
    {"2026-11-09T13:18:00", TransactionKind::Expense, 16800, "娱乐", "微信零钱", "网易云音乐 年卡"},
    {"2026-11-08T21:47:00", TransactionKind::Expense, 2860, "交通", "支付宝", "滴滴出行"},
    {"2026-11-08T18:20:00", TransactionKind::Income, 15800, "其他收入", "支付宝", "闲鱼 · 出旧键盘"},
    {"2026-11-07T10:05:00", TransactionKind::Expense, 4550, "医疗", "微信零钱", "药房 · 感冒药"},
    {"2026-11-06T22:33:00", TransactionKind::Expense, 19900, "教育", "支付宝", "得到 · 课程"},
    {"2026-11-05T09:00:00", TransactionKind::Income, 1860000, "工资", "招商银行储蓄卡", "工资 · 10 月"},
    {"2026-11-05T08:30:00", TransactionKind::Expense, 20000, "人情", "微信零钱", "同事结婚 · 红包"},
    {"2026-11-04T19:52:00", TransactionKind::Expense, 3280, "餐饮", "支付宝", "外卖 · 麻辣烫"},
    {"2026-11-03T16:41:00", TransactionKind::Expense, 8640, "日用", "招商银行储蓄卡", "洗衣液 · 纸巾"},
    {"2026-11-02T11:15:00", TransactionKind::Expense, 21460, "餐饮", "招商银行储蓄卡", "超市 · 食材"},
    {"2026-11-01T10:02:00", TransactionKind::Expense, 9900, "通讯", "招商银行储蓄卡", "手机话费 · 100 元包"},
};

// 6–10 月历史支出（每月 3 笔，合计 5,930/6,120/5,480/7,860/6,540，对应设计稿趋势数据）
constexpr TransactionSeed kHistory[] = {
    {"2026-06-09T20:00:00", TransactionKind::Expense, 320000, "住房", "招商银行储蓄卡", "房租 · 6 月"},
    {"2026-06-20T12:00:00", TransactionKind::Expense, 150000, "购物", "招商银行储蓄卡", "购物 · 月度采购"},
    {"2026-06-26T19:00:00", TransactionKind::Expense, 123000, "餐饮", "招商银行储蓄卡", "餐饮 · 月度汇总"},
    {"2026-07-09T20:00:00", TransactionKind::Expense, 320000, "住房", "招商银行储蓄卡", "房租 · 7 月"},
    {"2026-07-20T12:00:00", TransactionKind::Expense, 150000, "购物", "招商银行储蓄卡", "购物 · 月度采购"},
    {"2026-07-26T19:00:00", TransactionKind::Expense, 142000, "餐饮", "招商银行储蓄卡", "餐饮 · 月度汇总"},
    {"2026-08-09T20:00:00", TransactionKind::Expense, 320000, "住房", "招商银行储蓄卡", "房租 · 8 月"},
    {"2026-08-20T12:00:00", TransactionKind::Expense, 150000, "购物", "招商银行储蓄卡", "购物 · 月度采购"},
    {"2026-08-26T19:00:00", TransactionKind::Expense, 78000, "餐饮", "招商银行储蓄卡", "餐饮 · 月度汇总"},
    {"2026-09-09T20:00:00", TransactionKind::Expense, 320000, "住房", "招商银行储蓄卡", "房租 · 9 月"},
    {"2026-09-20T12:00:00", TransactionKind::Expense, 150000, "购物", "招商银行储蓄卡", "购物 · 月度采购"},
    {"2026-09-26T19:00:00", TransactionKind::Expense, 316000, "餐饮", "招商银行储蓄卡", "餐饮 · 月度汇总"},
    {"2026-10-09T20:00:00", TransactionKind::Expense, 320000, "住房", "招商银行储蓄卡", "房租 · 10 月"},
    {"2026-10-20T12:00:00", TransactionKind::Expense, 150000, "购物", "招商银行储蓄卡", "购物 · 月度采购"},
    {"2026-10-26T19:00:00", TransactionKind::Expense, 184000, "餐饮", "招商银行储蓄卡", "餐饮 · 月度汇总"},
};

}  // namespace

bool DemoSeeder::seed(QSqlDatabase db, QString *error)
{
    AccountRepository accounts(db);
    if (!accounts.listAll(true, error).isEmpty())
        return true;  // 已有数据，幂等跳过

    // 内置分类自带独立事务，必须在下面的事务之外先跑完（SQLite 不支持嵌套事务）
    CategoryRepository categories(db);
    if (!categories.seedBuiltins(error))
        return false;

    if (!db.transaction()) {
        if (error)
            *error = db.lastError().text();
        return false;
    }

    QHash<QString, qint64> accountIds;
    int sortOrder = 0;
    for (const AccountSeed &seedAccount : kAccounts) {
        Account account;
        account.name = QString::fromUtf8(seedAccount.name);
        account.type = seedAccount.type;
        account.openingBalance = Money::fromCents(seedAccount.openingCents);
        account.icon = QString::fromUtf8(seedAccount.icon);
        account.color = QString::fromLatin1(seedAccount.color);
        account.sortOrder = sortOrder++;

        const auto id = accounts.insert(account, error);
        if (!id.has_value()) {
            db.rollback();
            return false;
        }
        accountIds.insert(account.name, *id);
    }

    QHash<QString, qint64> categoryIds;
    for (CategoryKind kind : {CategoryKind::Expense, CategoryKind::Income}) {
        const QVector<Category> rows = categories.listByKind(kind, true, error);
        for (const Category &category : rows)
            categoryIds.insert(category.name, category.id);
    }

    TransactionRepository transactions(db);
    for (const TransactionSeed &seedTransaction : kTransactions) {
        Transaction transaction;
        transaction.kind = seedTransaction.kind;
        transaction.amount = Money::fromCents(seedTransaction.amountCents);
        transaction.accountId = accountIds.value(QString::fromUtf8(seedTransaction.account), 0);
        transaction.categoryId = categoryIds.value(QString::fromUtf8(seedTransaction.category));
        transaction.occurredAt = QDateTime::fromString(
            QString::fromLatin1(seedTransaction.occurredAt), Qt::ISODate);
        transaction.note = QString::fromUtf8(seedTransaction.note);

        if (transaction.accountId == 0 || !transaction.categoryId.has_value()
            || !transaction.occurredAt.isValid()) {
            if (error)
                *error = QStringLiteral("种子数据不完整：%1").arg(transaction.note);
            db.rollback();
            return false;
        }

        if (!transactions.insert(transaction, error).has_value()) {
            db.rollback();
            return false;
        }
    }

    for (const TransactionSeed &seedTransaction : kHistory) {
        Transaction transaction;
        transaction.kind = seedTransaction.kind;
        transaction.amount = Money::fromCents(seedTransaction.amountCents);
        transaction.accountId = accountIds.value(QString::fromUtf8(seedTransaction.account), 0);
        transaction.categoryId = categoryIds.value(QString::fromUtf8(seedTransaction.category));
        transaction.occurredAt = QDateTime::fromString(
            QString::fromLatin1(seedTransaction.occurredAt), Qt::ISODate);
        transaction.note = QString::fromUtf8(seedTransaction.note);

        if (!transactions.insert(transaction, error).has_value()) {
            db.rollback();
            return false;
        }
    }

    if (!db.commit()) {
        if (error)
            *error = db.lastError().text();
        db.rollback();
        return false;
    }

    // 预算另有自己的事务，放在主事务之外
    BudgetRepository budgets(db);
    if (!budgets.setMonthlyLimit(QStringLiteral("2026-11"), Money::fromCents(650000), error))
        return false;

    return true;
}

}  // namespace keep