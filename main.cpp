#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSqlQuery>
#include <QStringConverter>
#include <QVariant>

#include <cmath>

#include "core/data/AccountRepository.h"

#include "core/data/BackupService.h"
#include "core/data/BudgetRepository.h"
#include "core/data/ImportService.h"
#include "core/data/SettingsRepository.h"
#include "core/data/TagRepository.h"
#include "core/data/CategoryRepository.h"
#include "core/data/Database.h"
#include "core/data/DemoSeeder.h"
#include "core/data/Migrator.h"
#include "core/data/TransactionRepository.h"
#include "core/domain/Domain.h"
#include "core/model/LedgerViewModel.h"
#include "core/model/LedgerViewModel.h"

namespace {

int countUserTables(const QSqlDatabase &db)
{
    QSqlQuery query(db);
    const bool ok = query.exec(QStringLiteral(
        "SELECT COUNT(*) FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%'"));
    if (!ok || !query.next())
        return -1;
    return query.value(0).toInt();
}

// 自检：foreign_keys 是连接级开关，用「插入非法外键必须被拒绝」验证它真的生效
bool foreignKeysEnforced(QSqlDatabase db)
{
    db.transaction();
    QSqlQuery query(db);
    const bool inserted = query.exec(QStringLiteral(
        "INSERT INTO transactions(kind, amount_cents, account_id, occurred_at, created_at, updated_at) "
        "VALUES ('expense', 1, -1, '2000-01-01T00:00:00', '2000-01-01T00:00:00', '2000-01-01T00:00:00')"));
    db.rollback();
    return !inserted;
}

int g_failures = 0;

void expect(const QString &name, bool ok, const QString &detail = QString())
{
    const QString suffix = detail.isEmpty() ? QString() : QStringLiteral(" — ") + detail;
    qInfo().noquote() << QStringLiteral("%1  %2%3").arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), name, suffix);
    if (!ok)
        ++g_failures;
}

// 独立临时库跑一遍完整链路：迁移 → 种子 → 校验设计稿数字
int runSelfTest()
{
    const QString path = QDir(QDir::tempPath()).filePath(QStringLiteral("moyu_ledger_selftest.db"));
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")})
        QFile::remove(path + suffix);

    keep::Database database;
    QString error;
    if (!database.open(path, &error)) {
        qCritical().noquote() << QStringLiteral("自检失败：数据库打开失败 %1").arg(error);
        return 2;
    }

    {
    keep::Migrator migrator(database.handle());
    if (!migrator.migrate(&error)) {
        qCritical().noquote() << QStringLiteral("自检失败：迁移失败 %1").arg(error);
        return 3;
    }
    expect(QStringLiteral("schema v6 迁移完成（含理财/股票与每日收益）"), migrator.currentVersion() == 6,
           QString::number(migrator.currentVersion()));
    expect(QStringLiteral("建表 11 张"), countUserTables(database.handle()) == 11,
           QString::number(countUserTables(database.handle())));
    expect(QStringLiteral("foreign_keys 生效"), foreignKeysEnforced(database.handle()));

    keep::AccountRepository accounts(database.handle());
    keep::CategoryRepository categories(database.handle());
    keep::TransactionRepository transactions(database.handle());

    expect(QStringLiteral("内置分类种子"), categories.seedBuiltins(&error), error);
    expect(QStringLiteral("演示数据种子"), keep::DemoSeeder::seed(database.handle(), &error), error);
    expect(QStringLiteral("种子幂等"), keep::DemoSeeder::seed(database.handle(), &error), error);

    const QVector<keep::Category> allCategories = categories.listAll(true, &error);
    expect(QStringLiteral("分类 14 个"), allCategories.size() == 14, QString::number(allCategories.size()));
    expect(QStringLiteral("流水 32 笔（11 月 17 + 历史 15）"), transactions.countAll(&error) == 32,
           QString::number(transactions.countAll(&error)));

    const keep::MonthSummary summary = transactions.monthSummary(2026, 11, &error);
    expect(QStringLiteral("11 月收入 ¥18,758.00"), summary.income.cents() == 1875800, summary.income.format());
    expect(QStringLiteral("11 月支出 ¥5,907.90"), summary.expense.cents() == 590790, summary.expense.format());
    expect(QStringLiteral("11 月结余 ¥12,850.10"), summary.balance().cents() == 1285010, summary.balance().format());
    expect(QStringLiteral("本月记账 17 笔"), summary.count == 17, QString::number(summary.count));

    const auto netWorth = accounts.netWorth(&error);
    expect(QStringLiteral("净资产 ¥30,896.87"), netWorth.has_value() && netWorth->cents() == 3089687,
           netWorth ? netWorth->format() : error);

    struct ExpectedBalance
    {
        const char *name;
        qint64 cents;
    };
    constexpr ExpectedBalance expected[] = {
        {"招商银行储蓄卡", 2846032},
        {"支付宝", 321890},
        {"微信零钱", 84215},
        {"现金", 56000},
        {"招商银行信用卡", -218450},
    };
    const QVector<keep::AccountWithBalance> rows = accounts.listWithBalances(true, &error);
    expect(QStringLiteral("账户 5 个"), rows.size() == 5, QString::number(rows.size()));
    for (const ExpectedBalance &item : expected) {
        qint64 actual = 0;
        for (const keep::AccountWithBalance &row : rows) {
            if (row.account.name == QString::fromUtf8(item.name)) {
                actual = row.balance.cents();
                break;
            }
        }
        expect(QStringLiteral("余额 %1 %2").arg(QString::fromUtf8(item.name),
                                               keep::Money::fromCents(item.cents).format()),
               actual == item.cents, keep::Money::fromCents(actual).format());
    }

    const QVector<keep::CategoryTotal> shares =
        transactions.categoryShares(2026, 11, keep::TransactionKind::Expense, &error);
    expect(QStringLiteral("支出分类 10 个"), shares.size() == 10, QString::number(shares.size()));
    if (!shares.isEmpty()) {
        const keep::CategoryTotal &top = shares.first();
        expect(QStringLiteral("第一大分类 住房 ¥3,200.00 / 54.2%"),
               top.name == QStringLiteral("住房") && top.total.cents() == 320000
                   && std::abs(top.share - 0.5416) < 0.005,
               QStringLiteral("%1 %2 %3%").arg(top.name, top.total.format(),
                                               QString::number(top.share * 100.0, 'f', 1)));
    }

    const QVector<keep::MonthPoint> trend = transactions.monthlyTrend(2026, 11, 6, &error);
    expect(QStringLiteral("近 6 月趋势"), trend.size() == 6, QString::number(trend.size()));
    if (!trend.isEmpty()) {
        expect(QStringLiteral("趋势末月支出 ¥5,907.90"), trend.last().expense.cents() == 590790,
               trend.last().expense.format());
        expect(QStringLiteral("趋势首月 6 月支出 ¥5,930.00"), trend.first().expense.cents() == 593000,
               trend.first().expense.format());
    }

    // ---- 编辑与撤销（放到最后，避免影响前面的计数断言）----
    // 注意：SQLite 自增主键，撤销后重做会得到新 id，因此测试顺序固定为「增→改→删→撤销→重做」
    {
        keep::LedgerViewModel ledger(database.handle());
        const int baseCount = transactions.countAll(&error);

        const QVariantMap added = ledger.addTransaction(
            QStringLiteral("expense"), QStringLiteral("12.34"), ledger.defaultAccountId(),
            ledger.defaultCategoryId(QStringLiteral("expense")), QStringLiteral("2026-11-12"),
            QStringLiteral("自检新增"));
        const qint64 newId = added.value(QStringLiteral("id")).toLongLong();
        expect(QStringLiteral("新增一笔写入"),
               added.value(QStringLiteral("ok")).toBool() && newId > 0
                   && transactions.countAll(&error) == baseCount + 1,
               added.value(QStringLiteral("message")).toString());

        const QVariantMap updated = ledger.updateTransaction(
            newId, QStringLiteral("expense"), QStringLiteral("99.00"), ledger.defaultAccountId(),
            ledger.defaultCategoryId(QStringLiteral("expense")), QStringLiteral("2026-11-12"),
            QStringLiteral("自检编辑"));
        const auto edited = transactions.findById(newId, &error);
        expect(QStringLiteral("编辑金额 12.34 → 99.00"),
               updated.value(QStringLiteral("ok")).toBool() && edited.has_value()
                   && edited->amount.cents() == 9900 && edited->note == QStringLiteral("自检编辑"),
               edited ? edited->amount.format() : QStringLiteral("查找失败"));

        expect(QStringLiteral("编辑后可撤销"), ledger.canUndo(), ledger.undoText());
        ledger.undo();
        const auto reverted = transactions.findById(newId, &error);
        expect(QStringLiteral("撤销编辑回到 12.34"),
               reverted.has_value() && reverted->amount.cents() == 1234,
               reverted ? reverted->amount.format() : QStringLiteral("查找失败"));

        ledger.removeTransaction(newId);
        expect(QStringLiteral("删除后数量 -1"), transactions.countAll(&error) == baseCount,
               QString::number(transactions.countAll(&error)));
        ledger.undo();
        expect(QStringLiteral("撤销删除后恢复"), transactions.countAll(&error) == baseCount + 1,
               QString::number(transactions.countAll(&error)));

        // 清掉测试数据，保持库干净
        for (const keep::Transaction &row : transactions.listByMonth(2026, 11, &error)) {
            // 只按备注精确匹配，避免误删真实数据（例如话费 ¥99.00 也等于 9900 分）
            if (row.note == QStringLiteral("自检新增") || row.note == QStringLiteral("自检编辑"))
                transactions.remove(row.id);
        }
    }
    // ---- 搜索与筛选 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const auto expectCount = [&ledger](const QString &name, int expected) {
            expect(name, ledger.filteredCount() == expected,
                   QStringLiteral("实际 %1 笔").arg(ledger.filteredCount()));
        };

        ledger.setKeyword(QStringLiteral("星巴克"));
        expectCount(QStringLiteral("关键字「星巴克」→ 1 笔"), 1);
        expect(QStringLiteral("筛选摘要包含关键字"),
               ledger.filterSummary().contains(QStringLiteral("星巴克")), ledger.filterSummary());

        ledger.setKeyword(QStringLiteral("餐饮"));
        expectCount(QStringLiteral("关键字「餐饮」命中分类名 → 4 笔"), 4);

        ledger.setKeyword(QString());
        ledger.setAccountFilter(ledger.defaultAccountId());
        expectCount(QStringLiteral("按账户筛选（招行储蓄卡）→ 5 笔"), 5);

        ledger.setAccountFilter(-1);
        int shoppingId = -1;
        for (const QVariant &item : ledger.expenseCategories()) {
            const QVariantMap map = item.toMap();
            if (map.value(QStringLiteral("name")).toString() == QStringLiteral("购物"))
                shoppingId = map.value(QStringLiteral("id")).toInt();
        }
        ledger.setCategoryFilter(shoppingId);
        expectCount(QStringLiteral("按分类筛选（购物）→ 2 笔"), 2);

        ledger.clearFilter();
        expectCount(QStringLiteral("清空筛选 → 17 笔"), 17);
    }

    // ---- 账户管理（放到最后，会改变账户数量与净资产）----
    {
        keep::LedgerViewModel ledger(database.handle());
        const int baseAccounts = ledger.accountModel()->count();

        const QVariantMap added = ledger.addAccount(QStringLiteral("自检账户"),
                                                    QStringLiteral("savings"),
                                                    QStringLiteral("1000.00"), QString());
        const qint64 accountId = added.value(QStringLiteral("id")).toLongLong();
        expect(QStringLiteral("新增账户"),
               added.value(QStringLiteral("ok")).toBool() && accountId > 0
                   && ledger.accountModel()->count() == baseAccounts + 1,
               added.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("净资产 +¥1,000.00"),
               ledger.netWorthText() == QStringLiteral("¥31,896.87"), ledger.netWorthText());

        const QVariantMap updated = ledger.updateAccount(accountId, QStringLiteral("自检账户"),
                                                         QStringLiteral("savings"),
                                                         QStringLiteral("2000.00"), QString());
        expect(QStringLiteral("改期初余额 → 净资产 ¥32,896.87"),
               updated.value(QStringLiteral("ok")).toBool()
                   && ledger.netWorthText() == QStringLiteral("¥32,896.87"),
               ledger.netWorthText());

        const QVariantMap duplicate = ledger.addAccount(QStringLiteral("自检账户"),
                                                        QStringLiteral("cash"), QString(), QString());
        expect(QStringLiteral("重名账户被拒绝"), !duplicate.value(QStringLiteral("ok")).toBool(),
               duplicate.value(QStringLiteral("message")).toString());

        ledger.archiveAccount(accountId, true);
        expect(QStringLiteral("归档账户后隐藏且净资产回落"),
               ledger.accountModel()->count() == baseAccounts
                   && ledger.netWorthText() == QStringLiteral("¥30,896.87"),
               ledger.netWorthText());

        ledger.archiveAccount(accountId, false);
        expect(QStringLiteral("恢复归档账户"),
               ledger.accountModel()->count() == baseAccounts + 1, QString());
    }
    // ---- 报表：账户维度 + CSV 导出 ----
    {
        keep::LedgerViewModel ledger(database.handle());

        const QVector<keep::AccountTotal> totals = transactions.accountTotals(2026, 11, &error);
        qint64 expenseSum = 0;
        int txCount = 0;
        for (const keep::AccountTotal &row : totals) {
            expenseSum += row.expense.cents();
            txCount += row.count;
        }
        expect(QStringLiteral("账户维度支出合计 = 月度支出 ¥5,907.90"), expenseSum == 590790,
               keep::Money::fromCents(expenseSum).format());
        expect(QStringLiteral("账户维度笔数合计 17 笔"), txCount == 17, QString::number(txCount));

        const QString csvDir = QDir(QDir::tempPath()).filePath(QStringLiteral("keep_csv_test"));
        QDir(csvDir).removeRecursively();
        const QVariantMap csv = ledger.exportMonthCsv(csvDir);
        expect(QStringLiteral("CSV 导出成功"), csv.value(QStringLiteral("ok")).toBool(),
               csv.value(QStringLiteral("message")).toString());

        QFile csvFile(csv.value(QStringLiteral("path")).toString());
        if (csvFile.open(QIODevice::ReadOnly)) {
            const QByteArray data = csvFile.readAll();
            csvFile.close();
            expect(QStringLiteral("CSV 带 UTF-8 BOM（Excel 中文不乱码）"),
                   data.startsWith("\xEF\xBB\xBF"));
            expect(QStringLiteral("CSV 共 18 行（表头 + 17 笔）"), data.count('\n') == 18,
                   QString::number(data.count('\n')));
        } else {
            expect(QStringLiteral("CSV 文件可读"), false, QStringLiteral("打开失败"));
        }
        QDir(csvDir).removeRecursively();
    }
    // ---- 预算设置（总预算 + 分类预算）----
    {
        keep::LedgerViewModel ledger(database.handle());

        const QVariantMap total = ledger.setTotalBudget(QStringLiteral("7000.00"));
        expect(QStringLiteral("设置总预算 ¥7,000.00"),
               total.value(QStringLiteral("ok")).toBool()
                   && ledger.budgetLimitText() == QStringLiteral("¥7,000.00")
                   && ledger.budgetRemainText() == QStringLiteral("¥1,092.10"),
               ledger.budgetLimitText() + QStringLiteral(" / ") + ledger.budgetRemainText());

        int diningId = -1;
        for (const QVariant &item : ledger.expenseCategories()) {
            const QVariantMap map = item.toMap();
            if (map.value(QStringLiteral("name")).toString() == QStringLiteral("餐饮"))
                diningId = map.value(QStringLiteral("id")).toInt();
        }
        const QVariantMap category = ledger.setCategoryBudget(diningId, QStringLiteral("400.00"));

        bool found = false;
        for (const QVariant &row : ledger.budgetRows()) {
            const QVariantMap map = row.toMap();
            if (map.value(QStringLiteral("categoryId")).toInt() == diningId) {
                found = map.value(QStringLiteral("hasBudget")).toBool()
                        && map.value(QStringLiteral("spentText")).toString()
                               == QStringLiteral("¥308.40")
                        && map.value(QStringLiteral("ratioText")).toString() == QStringLiteral("77.1%");
            }
        }
        expect(QStringLiteral("设置分类预算（餐饮 ¥400 / 已花 ¥308.40 / 77.1%）"),
               category.value(QStringLiteral("ok")).toBool() && found,
               category.value(QStringLiteral("message")).toString());

        ledger.setCategoryBudget(diningId, QString());
        bool cleared = true;
        for (const QVariant &row : ledger.budgetRows()) {
            const QVariantMap map = row.toMap();
            if (map.value(QStringLiteral("categoryId")).toInt() == diningId
                && map.value(QStringLiteral("hasBudget")).toBool()) {
                cleared = false;
            }
        }
        expect(QStringLiteral("清除分类预算"), cleared);

        ledger.setTotalBudget(QStringLiteral("6500.00"));  // 复原为演示值，避免影响后续断言
    }
    // ---- 报表范围 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        expect(QStringLiteral("报表默认本月：支出 ¥5,907.90 / 17 笔"),
               ledger.reportExpenseText() == QStringLiteral("¥5,907.90")
                   && ledger.reportCountText() == QStringLiteral("17"),
               ledger.reportExpenseText() + QStringLiteral(" / ") + ledger.reportCountText());

        ledger.setReportRange(QStringLiteral("3m"));
        expect(QStringLiteral("近 3 月支出 ¥20,307.90（7,860+6,540+5,907.90）"),
               ledger.reportExpenseText() == QStringLiteral("¥20,307.90"),
               ledger.reportExpenseText());
        expect(QStringLiteral("近 3 月笔数 23"), ledger.reportCountText() == QStringLiteral("23"),
               ledger.reportCountText());
        expect(QStringLiteral("近 3 月分类占比合计 100%"),
               [&ledger] {
                   double sum = 0.0;
                   for (const QVariant &item : ledger.reportShares())
                       sum += item.toMap().value(QStringLiteral("share")).toDouble();
                   return std::abs(sum - 1.0) < 0.005;
               }(),
               QStringLiteral("占比合计校验"));

        ledger.setReportRange(QStringLiteral("6m"));
        expect(QStringLiteral("近 6 月支出 ¥37,837.90"), ledger.reportExpenseText() == QStringLiteral("¥37,837.90"),
               ledger.reportExpenseText());
        expect(QStringLiteral("近 6 月笔数 32"), ledger.reportCountText() == QStringLiteral("32"),
               ledger.reportCountText());
        expect(QStringLiteral("近 6 月趋势 6 个点"), ledger.reportTrend().size() == 6,
               QString::number(ledger.reportTrend().size()));

        ledger.setReportRange(QStringLiteral("year"));
        expect(QStringLiteral("本年（1-11 月）支出等于近 6 月总额"),
               ledger.reportExpenseText() == QStringLiteral("¥37,837.90"),
               ledger.reportExpenseText());

        ledger.setReportRange(QStringLiteral("month"));
        expect(QStringLiteral("切回本月"), ledger.reportExpenseText() == QStringLiteral("¥5,907.90"),
               ledger.reportExpenseText());
    }
    // ---- 账单导入（UTF-8 BOM / GBK 两种编码 + 去重）----
    {
        keep::LedgerViewModel ledger(database.handle());
        const QString dir = QDir(QDir::tempPath()).filePath(QStringLiteral("keep_import_test"));
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);

        const QString csvText = QStringLiteral(
            "微信支付账单明细,,,,,,,,\n"
            "微信昵称：[测试],,,,,,,,\n"
            "----------------------微信支付账单明细列表--------------------,,,,,,,,\n"
            "交易时间,交易类型,交易对方,商品,收/支,金额(元),支付方式,当前状态,交易单号,商户单号,备注\n"
            "2026-12-13 08:30:00,商户消费,瑞幸咖啡,生椰拿铁,支出,¥19.90,零钱,支付成功,WX001,M001,\n"
            "2026-12-13 12:10:00,商户消费,美团,外卖,支出,¥32.00,零钱,支付成功,WX002,M002,\n"
            "2026-12-13 18:00:00,转账,张三,转账,收入,¥200.00,零钱,已存入零钱,WX003,M003,还钱\n"
            "2026-12-14 09:00:00,商户消费,某商户,退款,支出,¥50.00,零钱,已全额退款,WX004,M004,\n");

        const QString utf8Path = QDir(dir).filePath(QStringLiteral("wechat_utf8.csv"));
        QFile utf8File(utf8Path);
        utf8File.open(QIODevice::WriteOnly);
        utf8File.write("\xEF\xBB\xBF");
        utf8File.write(csvText.toUtf8());
        utf8File.close();

        const QVariantMap preview = ledger.previewImportCsv(utf8Path);
        expect(QStringLiteral("识别微信 CSV：3 笔有效（退款行被过滤）"),
               preview.value(QStringLiteral("ok")).toBool()
                   && preview.value(QStringLiteral("total")).toInt() == 3,
               preview.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("预览金额：支出 ¥51.90 / 收入 ¥200.00"),
               preview.value(QStringLiteral("expenseText")).toString() == QStringLiteral("¥51.90")
                   && preview.value(QStringLiteral("incomeText")).toString() == QStringLiteral("¥200.00"),
               preview.value(QStringLiteral("expenseText")).toString() + QStringLiteral(" / ")
                   + preview.value(QStringLiteral("incomeText")).toString());

        const int countBefore = transactions.countAll(&error);
        const QVariantMap imported =
            ledger.importCsv(utf8Path, ledger.defaultAccountId(), -1);
        expect(QStringLiteral("导入 3 笔"),
               imported.value(QStringLiteral("ok")).toBool()
                   && imported.value(QStringLiteral("imported")).toInt() == 3
                   && transactions.countAll(&error) == countBefore + 3,
               imported.value(QStringLiteral("message")).toString());

        const QVariantMap again = ledger.importCsv(utf8Path, ledger.defaultAccountId(), -1);
        expect(QStringLiteral("重复导入全部跳过（按交易单号去重）"),
               again.value(QStringLiteral("imported")).toInt() == 0
                   && again.value(QStringLiteral("skipped")).toInt() == 3,
               again.value(QStringLiteral("message")).toString());

        // GBK 编码版本
        const QString gbkPath = QDir(dir).filePath(QStringLiteral("alipay_gbk.csv"));
        QStringEncoder encoder(QStringConverter::System);
        QFile gbkFile(gbkPath);
        gbkFile.open(QIODevice::WriteOnly);
        gbkFile.write(encoder(csvText));
        gbkFile.close();
        const QVariantMap gbkPreview = ledger.previewImportCsv(gbkPath);
        expect(QStringLiteral("GBK 编码账单也能识别"),
               gbkPreview.value(QStringLiteral("ok")).toBool()
                   && gbkPreview.value(QStringLiteral("total")).toInt() == 3,
               gbkPreview.value(QStringLiteral("message")).toString());

        QDir(dir).removeRecursively();
    }
    // ---- 报表环比 / 同比 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        expect(QStringLiteral("本月支出环比 ↓9.7%（对 10 月 ¥6,540）"),
               ledger.reportExpenseMoM() == QStringLiteral("↓ 9.7%"), ledger.reportExpenseMoM());
        expect(QStringLiteral("本月支出同比无数据 → —"), ledger.reportExpenseYoY() == QStringLiteral("—"),
               ledger.reportExpenseYoY());
        expect(QStringLiteral("本月收入环比无上月收入 → —"), ledger.reportIncomeMoM() == QStringLiteral("—"),
               ledger.reportIncomeMoM());
        expect(QStringLiteral("月度明细 1 行（本月）"), ledger.reportMonthlyRows().size() == 1,
               QString::number(ledger.reportMonthlyRows().size()));

        ledger.setReportRange(QStringLiteral("3m"));
        expect(QStringLiteral("近 3 月月度明细 3 行"), ledger.reportMonthlyRows().size() == 3,
               QString::number(ledger.reportMonthlyRows().size()));
        expect(QStringLiteral("文档目录可获取"), !ledger.documentsDirectory().isEmpty(),
               ledger.documentsDirectory());
        ledger.setReportRange(QStringLiteral("month"));
    }
    // ---- 预算超配提示 + 按月复制 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        keep::BudgetRepository budgetRepo(database.handle());

        int diningId = -1;
        int shoppingId = -1;
        for (const QVariant &item : ledger.expenseCategories()) {
            const QVariantMap map = item.toMap();
            if (map.value(QStringLiteral("name")).toString() == QStringLiteral("餐饮"))
                diningId = map.value(QStringLiteral("id")).toInt();
            if (map.value(QStringLiteral("name")).toString() == QStringLiteral("购物"))
                shoppingId = map.value(QStringLiteral("id")).toInt();
        }

        ledger.setTotalBudget(QStringLiteral("1000.00"));
        ledger.setCategoryBudget(diningId, QStringLiteral("700.00"));
        ledger.setCategoryBudget(shoppingId, QStringLiteral("400.00"));

        expect(QStringLiteral("分类预算合计超过总预算被识别"), ledger.budgetOverAllocated(),
               ledger.budgetCategorySumText());
        expect(QStringLiteral("超配提示文案包含「超过」"),
               ledger.budgetWarningText().contains(QStringLiteral("超过")), ledger.budgetWarningText());
        expect(QStringLiteral("分类预算合计 ¥1,100.00"),
               ledger.budgetCategorySumText() == QStringLiteral("¥1,100.00"),
               ledger.budgetCategorySumText());

        const QVariantMap copyResult = ledger.copyBudgetToNextMonth();
        const auto nextLimit = budgetRepo.monthlyLimit(QStringLiteral("2026-12"));
        const auto nextCategories = budgetRepo.categoryBudgets(QStringLiteral("2026-12"));
        expect(QStringLiteral("复制到下一月：总预算 + 2 个分类预算"),
               copyResult.value(QStringLiteral("ok")).toBool() && nextLimit.has_value()
                   && nextLimit->cents() == 100000 && nextCategories.size() == 2,
               copyResult.value(QStringLiteral("message")).toString());

        const QVariantMap fromPrevious = ledger.copyBudgetFromPreviousMonth();
        expect(QStringLiteral("上月无预算时复制返回失败提示"),
               !fromPrevious.value(QStringLiteral("ok")).toBool(),
               fromPrevious.value(QStringLiteral("message")).toString());

        // 复原
        ledger.setCategoryBudget(diningId, QString());
        ledger.setCategoryBudget(shoppingId, QString());
        ledger.setTotalBudget(QStringLiteral("6500.00"));
    }
    // ---- 标签：录入 / 覆盖 / 报表维度 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const int accountId = ledger.defaultAccountId();
        const int categoryId = ledger.defaultCategoryId(QStringLiteral("expense"));

        const QVariantMap added = ledger.addTransaction(
            QStringLiteral("expense"), QStringLiteral("88.00"), accountId, categoryId,
            QStringLiteral("2026-11-15"), QStringLiteral("标签自检"), QStringLiteral("商务,报销"));
        expect(QStringLiteral("带标签记一笔"), added.value(QStringLiteral("ok")).toBool(),
               added.value(QStringLiteral("message")).toString());
        const qint64 tagTxId = added.value(QStringLiteral("id")).toLongLong();

        keep::TagRepository tagRepository(database.handle());
        const QStringList tags = tagRepository.tagsFor(tagTxId);
        expect(QStringLiteral("标签写入两个（商务 / 报销）"),
               tags.size() == 2 && tags.contains(QStringLiteral("商务"))
                   && tags.contains(QStringLiteral("报销")),
               tags.join(QStringLiteral(",")));
        expect(QStringLiteral("标签建议列表包含新标签"),
               ledger.allTags().contains(QStringLiteral("商务")),
               ledger.allTags().join(QStringLiteral(",")));

        ledger.updateTransaction(tagTxId, QStringLiteral("expense"), QStringLiteral("88.00"), accountId,
                                 categoryId, QStringLiteral("2026-11-15"), QStringLiteral("标签自检"),
                                 QStringLiteral("商务"));
        const QStringList afterEdit = tagRepository.tagsFor(tagTxId);
        expect(QStringLiteral("编辑后标签被覆盖为一个"),
               afterEdit.size() == 1 && afterEdit.first() == QStringLiteral("商务"),
               afterEdit.join(QStringLiteral(",")));

        bool hasTagShare = false;
        for (const QVariant &row : ledger.reportTagShares()) {
            if (row.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("商务"))
                hasTagShare = true;
        }
        expect(QStringLiteral("报表标签维度出现「商务」"), hasTagShare);

        ledger.removeTransaction(tagTxId);
        expect(QStringLiteral("删除后标签级联清理"), tagRepository.tagsFor(tagTxId).isEmpty());
    }
    // ---- 记一笔面板：新建大类 / 小类 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        int diningId = -1;
        for (const QVariant &big : ledger.expenseCategoryTree()) {
            if (big.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("餐饮"))
                diningId = big.toMap().value(QStringLiteral("id")).toInt();
        }

        const QVariantMap newSub = ledger.addCategory(QStringLiteral("expense"),
                                                      QStringLiteral("夜宵自检"), QString(),
                                                      diningId);
        expect(QStringLiteral("在「餐饮」下新建小类"),
               newSub.value(QStringLiteral("ok")).toBool(),
               newSub.value(QStringLiteral("message")).toString());

        bool subFound = false;
        for (const QVariant &big : ledger.expenseCategoryTree()) {
            const QVariantMap row = big.toMap();
            if (row.value(QStringLiteral("id")).toInt() != diningId)
                continue;
            for (const QVariant &child : row.value(QStringLiteral("children")).toList()) {
                if (child.toMap().value(QStringLiteral("name")).toString()
                    == QStringLiteral("夜宵自检"))
                    subFound = true;
            }
        }
        expect(QStringLiteral("新小类出现在分类树里"), subFound);

        const QVariantMap dupSub = ledger.addCategory(QStringLiteral("expense"),
                                                      QStringLiteral("夜宵自检"), QString(),
                                                      diningId);
        expect(QStringLiteral("同一大类下重名小类被拒绝"),
               !dupSub.value(QStringLiteral("ok")).toBool(),
               dupSub.value(QStringLiteral("message")).toString());

        const QVariantMap newBig = ledger.addCategory(QStringLiteral("expense"),
                                                      QStringLiteral("宠物自检"), QString());
        expect(QStringLiteral("新建大类并返回 id"),
               newBig.value(QStringLiteral("ok")).toBool()
                   && newBig.contains(QStringLiteral("id")),
               newBig.value(QStringLiteral("message")).toString());
    }

    // ---- 记一笔面板：大类预算卡（含小类聚合）----
    {
        keep::LedgerViewModel ledger(database.handle());
        int housingId = -1;
        int diningId = -1;
        int lunchId = -1;
        for (const QVariant &big : ledger.expenseCategoryTree()) {
            const QVariantMap row = big.toMap();
            if (row.value(QStringLiteral("name")).toString() == QStringLiteral("住房"))
                housingId = row.value(QStringLiteral("id")).toInt();
            if (row.value(QStringLiteral("name")).toString() == QStringLiteral("餐饮")) {
                diningId = row.value(QStringLiteral("id")).toInt();
                for (const QVariant &child : row.value(QStringLiteral("children")).toList()) {
                    if (child.toMap().value(QStringLiteral("name")).toString() == QStringLiteral("午餐"))
                        lunchId = child.toMap().value(QStringLiteral("id")).toInt();
                }
            }
        }

        // 未设预算
        const QVariantMap noBudget = ledger.categoryBudgetSummary(housingId);
        expect(QStringLiteral("大类未设预算时 hasBudget=false"),
               noBudget.value(QStringLiteral("hasBudget")).toBool() == false
                   && !noBudget.value(QStringLiteral("spentText")).toString().isEmpty(),
               noBudget.value(QStringLiteral("spentText")).toString());

        // 设置预算后
        ledger.setCategoryBudget(housingId, QStringLiteral("4000.00"));
        const QVariantMap withBudget = ledger.categoryBudgetSummary(housingId);
        expect(QStringLiteral("大类预算卡：显示预算与剩余"),
               withBudget.value(QStringLiteral("hasBudget")).toBool()
                   && withBudget.value(QStringLiteral("budgetText")).toString()
                          == QStringLiteral("¥4,000.00")
                   && !withBudget.value(QStringLiteral("remainText")).toString().isEmpty(),
               withBudget.value(QStringLiteral("spentText")).toString() + QStringLiteral(" / ")
                   + withBudget.value(QStringLiteral("remainText")).toString());

        // 小类支出计入大类合计
        const auto centsOf = [](const QString &text) -> qint64 {
            QString clean = text;
            clean.remove(QChar(0x00A5));
            clean.remove(QChar(0xFFE5));
            clean.remove(QLatin1Char(','));
            const auto money = keep::Money::fromYuan(clean);
            return money.has_value() ? money->cents() : 0;
        };
        const qint64 diningBefore = centsOf(ledger.categoryBudgetSummary(diningId)
                                                .value(QStringLiteral("spentText")).toString());

        QVariantMap entry;
        entry.insert(QStringLiteral("kind"), QStringLiteral("expense"));
        entry.insert(QStringLiteral("amountText"), QStringLiteral("10.00"));
        entry.insert(QStringLiteral("accountId"), ledger.defaultAccountId());
        entry.insert(QStringLiteral("categoryId"), lunchId);
        entry.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-13"));
        entry.insert(QStringLiteral("timeText"), QStringLiteral("12:34"));
        entry.insert(QStringLiteral("note"), QStringLiteral("小类聚合自检"));
        ledger.saveEntry(entry);

        const qint64 diningAfter = centsOf(ledger.categoryBudgetSummary(diningId)
                                               .value(QStringLiteral("spentText")).toString());
        expect(QStringLiteral("小类支出计入大类合计（+¥10）"), diningAfter == diningBefore + 1000,
               QStringLiteral("%1 -> %2").arg(diningBefore).arg(diningAfter));

        // 超支判定
        ledger.setCategoryBudget(diningId, QStringLiteral("100.00"));
        expect(QStringLiteral("超支时 overLimit=true"),
               ledger.categoryBudgetSummary(diningId)
                   .value(QStringLiteral("overLimit")).toBool());
    }

    // ---- 投资类账户：每日收益 / 佣金 / 修复 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const auto centsOf = [](const QString &text) -> qint64 {
            QString clean = text;
            clean.remove(QChar(0x00A5));
            clean.remove(QChar(0xFFE5));
            clean.remove(QLatin1Char(','));
            clean.remove(QLatin1Char('+'));
            clean.remove(QLatin1Char('-'));
            const auto money = keep::Money::fromYuan(clean);
            return money.has_value() ? money->cents() : 0;
        };
        const auto balanceOf = [&](const QString &name) -> qint64 {
            for (const keep::AccountWithBalance &row : accounts.listWithBalances()) {
                if (row.account.name == name)
                    return row.balance.cents();
            }
            return -999999;
        };

        // 分类种子
        expect(QStringLiteral("已补种「投资收益 / 投资亏损 / 手续费」分类"),
               categories.findByName(QStringLiteral("投资收益"), keep::CategoryKind::Income).has_value()
                   && categories.findByName(QStringLiteral("投资亏损"), keep::CategoryKind::Expense).has_value()
                   && categories.findByName(QStringLiteral("手续费"), keep::CategoryKind::Expense).has_value());

        const QVariantMap stock = ledger.addAccount(QStringLiteral("股票自检"),
                                                    QStringLiteral("stock"),
                                                    QStringLiteral("10000.00"), QString());
        expect(QStringLiteral("可创建「股票」类型账户"), stock.value(QStringLiteral("ok")).toBool(),
               stock.value(QStringLiteral("message")).toString());
        const int stockId = stock.value(QStringLiteral("id")).toInt();
        expect(QStringLiteral("股票账户被识别为投资类"), ledger.isInvestmentAccount(stockId));

        // 1) 记录当日收益 + 佣金
        QVariantMap day1;
        day1.insert(QStringLiteral("accountId"), stockId);
        day1.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-13"));
        day1.insert(QStringLiteral("mode"), QStringLiteral("income"));
        day1.insert(QStringLiteral("incomeText"), QStringLiteral("25.50"));
        day1.insert(QStringLiteral("commissionText"), QStringLiteral("5.00"));
        day1.insert(QStringLiteral("note"), QStringLiteral("每日收益自检"));
        const QVariantMap saved1 = ledger.saveDailyRecord(day1);
        expect(QStringLiteral("每日收益保存成功（收益 +25.50 / 佣金 5.00）"),
               saved1.value(QStringLiteral("ok")).toBool(),
               saved1.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("账户余额 = 期初 + 收益 − 佣金"),
               balanceOf(QStringLiteral("股票自检")) == 1000000 + 2550 - 500,
               QString::number(balanceOf(QStringLiteral("股票自检"))));
        expect(QStringLiteral("联动生成收益收入流水 + 佣金支出流水"),
               ledger.dailyRecords(stockId).size() == 1
                   && ledger.incomeText() != QStringLiteral("¥0.00"),
               ledger.incomeText() + QStringLiteral(" / ") + ledger.expenseText());

        // 2) 修复当日收益（改收益与佣金）
        const int recordId = ledger.dailyRecords(stockId).first().toMap()
                                 .value(QStringLiteral("id")).toInt();
        QVariantMap fix = day1;
        fix.insert(QStringLiteral("recordId"), recordId);
        fix.insert(QStringLiteral("incomeText"), QStringLiteral("30.00"));
        fix.insert(QStringLiteral("commissionText"), QStringLiteral("2.00"));
        const QVariantMap fixed = ledger.saveDailyRecord(fix);
        expect(QStringLiteral("每日收益修复成功（+30.00 / 佣金 2.00）"),
               fixed.value(QStringLiteral("ok")).toBool()
                   && balanceOf(QStringLiteral("股票自检")) == 1000000 + 3000 - 200,
               QString::number(balanceOf(QStringLiteral("股票自检"))));

        // 3) 用市值推算次日收益
        QVariantMap day2;
        day2.insert(QStringLiteral("accountId"), stockId);
        day2.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-14"));
        day2.insert(QStringLiteral("mode"), QStringLiteral("value"));
        day2.insert(QStringLiteral("valueText"), QStringLiteral("10050.00"));
        day2.insert(QStringLiteral("commissionText"), QStringLiteral("1.00"));
        const QVariantMap saved2 = ledger.saveDailyRecord(day2);
        const QVariantList records = ledger.dailyRecords(stockId);
        expect(QStringLiteral("市值推算收益：10050 − 10028 + 1 = 23.00"),
               saved2.value(QStringLiteral("ok")).toBool() && records.size() == 2
                   && records.first().toMap().value(QStringLiteral("incomeCents")).toLongLong() == 2300,
               records.isEmpty() ? QString() : records.first().toMap().value(QStringLiteral("incomeText")).toString());

        // 4) 亏损日：记为支出（投资亏损）
        QVariantMap day3;
        day3.insert(QStringLiteral("accountId"), stockId);
        day3.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-15"));
        day3.insert(QStringLiteral("mode"), QStringLiteral("income"));
        day3.insert(QStringLiteral("incomeText"), QStringLiteral("-15.00"));
        const QVariantMap saved3 = ledger.saveDailyRecord(day3);
        expect(QStringLiteral("亏损日记为支出（余额 −15）"),
               saved3.value(QStringLiteral("ok")).toBool()
                   && balanceOf(QStringLiteral("股票自检")) == 1000000 + 3000 - 200 + 2300 - 100 - 1500,
               QString::number(balanceOf(QStringLiteral("股票自检"))));

        // 5) 删除某日记录 → 联动流水一起删除
        const int lastId = ledger.dailyRecords(stockId).first().toMap()
                               .value(QStringLiteral("id")).toInt();
        const QVariantMap removed = ledger.removeDailyRecord(lastId);
        expect(QStringLiteral("删除每日记录并联动删除流水"),
               removed.value(QStringLiteral("ok")).toBool()
                   && ledger.dailyRecords(stockId).size() == 2,
               removed.value(QStringLiteral("message")).toString());

        // 6) 非投资账户拒绝
        QVariantMap bank;
        bank.insert(QStringLiteral("accountId"), ledger.defaultAccountId());
        bank.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-13"));
        bank.insert(QStringLiteral("mode"), QStringLiteral("income"));
        bank.insert(QStringLiteral("incomeText"), QStringLiteral("1.00"));
        expect(QStringLiteral("储蓄卡不能记录每日收益"),
               !ledger.saveDailyRecord(bank).value(QStringLiteral("ok")).toBool());

        // 7) 汇总
        const QVariantMap summary = ledger.investmentSummary(stockId);
        expect(QStringLiteral("投资汇总：收益与佣金合计正确"),
               centsOf(summary.value(QStringLiteral("incomeText")).toString()) == 3000 + 2300
                   && centsOf(summary.value(QStringLiteral("commissionText")).toString()) == 200 + 100,
               summary.value(QStringLiteral("incomeText")).toString() + QStringLiteral(" / ")
                   + summary.value(QStringLiteral("commissionText")).toString());
    }

    // ---- 全量备份导出 / 导入 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const QString backupPath = QDir::temp().filePath(QStringLiteral("ka-full-backup-test.json"));
        QFile::remove(backupPath);

        const QVariantMap exported = ledger.exportBackup(backupPath);
        expect(QStringLiteral("全量备份导出成功"),
               exported.value(QStringLiteral("ok")).toBool()
                   && QFileInfo(backupPath).size() > 500,
               exported.value(QStringLiteral("message")).toString());

        // 导出的 JSON 必须包含账户、预算、流水
        bool hasAccounts = false;
        bool hasBudgets = false;
        bool hasTransactions = false;
        {
            QFile file(backupPath);
            if (file.open(QIODevice::ReadOnly)) {
                const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
                const QJsonObject data = root.value(QStringLiteral("data")).toObject();
                hasAccounts = data.contains(QStringLiteral("accounts"))
                              && !data.value(QStringLiteral("accounts")).toArray().isEmpty();
                hasBudgets = data.contains(QStringLiteral("budgets"));
                hasTransactions = data.contains(QStringLiteral("transactions"))
                                  && !data.value(QStringLiteral("transactions")).toArray().isEmpty();
            }
        }
        expect(QStringLiteral("备份内容包含账户 / 预算 / 流水"),
               hasAccounts && hasBudgets && hasTransactions);

        const int accountsBefore = accounts.listAll(true).size();
        const int transactionsBefore = transactions.countAll();
        const QString incomeBefore = ledger.incomeText();

        const QVariantMap imported = ledger.importBackup(backupPath);
        expect(QStringLiteral("全量备份导入成功"),
               imported.value(QStringLiteral("ok")).toBool(),
               imported.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("导入后账户数一致"),
               accounts.listAll(true).size() == accountsBefore,
               QString::number(accounts.listAll(true).size()));
        expect(QStringLiteral("导入后流水数一致"),
               transactions.countAll() == transactionsBefore,
               QString::number(transactions.countAll()));
        expect(QStringLiteral("导入后本月收入一致"), ledger.incomeText() == incomeBefore,
               ledger.incomeText());

        // 非本应用备份文件要被拒绝
        const QString bogusPath = QDir::temp().filePath(QStringLiteral("ka-bogus-backup.json"));
        {
            QFile file(bogusPath);
            if (file.open(QIODevice::WriteOnly))
                file.write("{\"format\":\"something_else\",\"data\":{}}");
        }
        expect(QStringLiteral("非本应用备份文件被拒绝"),
               !ledger.importBackup(bogusPath).value(QStringLiteral("ok")).toBool());
        QFile::remove(bogusPath);
        QFile::remove(backupPath);
    }

    // ---- 隐私模式：金额隐藏为 * ----
    {
        keep::LedgerViewModel ledger(database.handle());
        ledger.setPrivacyMode(true);
        const QString maskedIncome = ledger.incomeText();
        const QString maskedExpense = ledger.expenseText();
        const QString maskedBalance = ledger.balanceText();
        bool anyDigit = false;
        for (const QString &text : {maskedIncome, maskedExpense, maskedBalance}) {
            for (const QChar ch : text) {
                if (ch.isDigit())
                    anyDigit = true;
            }
        }
        expect(QStringLiteral("隐私模式：金额数字全部变成 *"),
               maskedIncome.contains(QLatin1Char('*')) && !anyDigit,
               maskedIncome + QStringLiteral(" / ") + maskedExpense);

        ledger.setPrivacyMode(false);
        expect(QStringLiteral("隐私模式关闭后恢复真实金额"),
               !ledger.incomeText().contains(QLatin1Char('*'))
                   && ledger.incomeText().contains(QLatin1Char('8')),
               ledger.incomeText());
    }

    // ---- 主题持久化 + 预算提醒 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        expect(QStringLiteral("默认主题为浅色"), !ledger.themeDark());
        ledger.saveThemeDark(true);
        keep::LedgerViewModel another(database.handle());
        expect(QStringLiteral("主题选择写入 settings 并可读回"), another.themeDark());
        ledger.saveThemeDark(false);

        ledger.setTotalBudget(QStringLiteral("1000.00"));  // 支出 5,907.90 → 超支
        expect(QStringLiteral("超支时提醒等级 2 且文案含「超支」"),
               ledger.budgetAlertLevel() == 2
                   && ledger.budgetAlertText().contains(QStringLiteral("超支")),
               ledger.budgetAlertText());

        ledger.setTotalBudget(QStringLiteral("7000.00"));  // 84.4% → 不提醒
        expect(QStringLiteral("占比 84.4% 不触发提醒"), ledger.budgetAlertLevel() == 0,
               ledger.budgetAlertText());

        ledger.setTotalBudget(QStringLiteral("6500.00"));  // 90.9% → 临限提醒
        expect(QStringLiteral("占比 90.9% 触发临限提醒"),
               ledger.budgetAlertLevel() == 1
                   && ledger.budgetAlertText().contains(QStringLiteral("剩余")),
               ledger.budgetAlertText());
    }
    // ---- 导入字段增强（无收/支列 + 商户单号 + 对方账号）+ 报表 PDF ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const QString dir = QDir(QDir::tempPath()).filePath(QStringLiteral("keep_import_fields"));
        QDir(dir).removeRecursively();
        QDir().mkpath(dir);

        const QString csv = QStringLiteral(
            "交易时间,交易类型,交易对方,商品,金额(元),对方账号,商户单号,备注\n"
            "2026-12-20 09:00:00,收入,李四,还款,¥120.00,alipay_li@x.com,MB001,\n"
            "2026-12-21 10:00:00,支出,,咖啡,¥18.00,,MB002,\n");
        const QString path = QDir(dir).filePath(QStringLiteral("no_direction.csv"));
        QFile file(path);
        file.open(QIODevice::WriteOnly);
        file.write("\xEF\xBB\xBF");
        file.write(csv.toUtf8());
        file.close();

        const QVariantMap preview = ledger.previewImportCsv(path);
        const QVariantList rows = preview.value(QStringLiteral("rows")).toList();
        bool typeOk = rows.size() == 2;
        bool payeeFallback = false;
        if (rows.size() == 2) {
            const QVariantMap second = rows.at(1).toMap();
            payeeFallback = second.value(QStringLiteral("payee")).toString() == QStringLiteral("咖啡")
                            || second.value(QStringLiteral("note")).toString() == QStringLiteral("咖啡");
        }
        expect(QStringLiteral("无「收/支」列时按交易类型判断收支"),
               preview.value(QStringLiteral("ok")).toBool() && typeOk
                   && preview.value(QStringLiteral("expenseCount")).toInt() == 1
                   && preview.value(QStringLiteral("incomeCount")).toInt() == 1,
               preview.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("对方账号/商品兜底不丢信息"), payeeFallback);

        const int before = transactions.countAll(&error);
        const QVariantMap imported = ledger.importCsv(path, ledger.defaultAccountId(), -1);
        const QVariantMap again = ledger.importCsv(path, ledger.defaultAccountId(), -1);
        expect(QStringLiteral("商户单号作为去重键"),
               imported.value(QStringLiteral("imported")).toInt() == 2
                   && again.value(QStringLiteral("skipped")).toInt() == 2
                   && transactions.countAll(&error) == before + 2,
               again.value(QStringLiteral("message")).toString());

        const QVariantMap pdf = ledger.exportReportPdf(dir);
        const QFileInfo pdfInfo(pdf.value(QStringLiteral("path")).toString());
        bool pdfValid = pdf.value(QStringLiteral("ok")).toBool() && pdfInfo.exists()
                        && pdfInfo.size() > 512;
        if (pdfValid) {
            QFile pdfFile(pdfInfo.filePath());
            if (pdfFile.open(QIODevice::ReadOnly)) {
                const QByteArray head = pdfFile.readAll();
                pdfFile.close();
                // 必须是合法 PDF（%PDF 头）且含字体资源
                pdfValid = head.startsWith("%PDF") && head.contains("/Font");
            } else {
                pdfValid = false;
            }
        }
        expect(QStringLiteral("报表导出 PDF 成功（含 %PDF 头与字体）"), pdfValid,
               pdf.value(QStringLiteral("message")).toString());

        QDir(dir).removeRecursively();
    }
    // ---- 定期账单：规则 / 生成幂等 / 暂停 / 删除 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const int accountId = ledger.defaultAccountId();
        const int categoryId = ledger.defaultCategoryId(QStringLiteral("expense"));

        const QVariantMap added = ledger.addRecurring(QStringLiteral("expense"),
                                                      QStringLiteral("3200.00"), accountId,
                                                      categoryId, QStringLiteral("房租自检"), 9);
        expect(QStringLiteral("新增定期账单规则"),
               added.value(QStringLiteral("ok")).toBool()
                   && ledger.recurringRules().size() == 1,
               added.value(QStringLiteral("message")).toString());

        const int before = transactions.countAll(&error);
        const QVariantMap first = ledger.generateRecurring();
        const QVariantMap second = ledger.generateRecurring();
        expect(QStringLiteral("定期账单按月生成且幂等"),
               first.value(QStringLiteral("ok")).toBool()
                   && second.value(QStringLiteral("ok")).toBool() == false
                   && transactions.countAll(&error) == before + 1,
               second.value(QStringLiteral("message")).toString());

        // 找到生成的那笔，验证内容
        bool found = false;
        for (const keep::Transaction &row : transactions.listByMonth(2026, 11)) {
            if (row.note == QStringLiteral("房租自检") && row.amount.cents() == 320000
                && row.externalId.startsWith(QStringLiteral("recurring:"))) {
                found = true;
            }
        }
        expect(QStringLiteral("生成的流水金额与备注正确"), found);

        const qint64 ruleId = ledger.recurringRules().first().toMap().value(QStringLiteral("id")).toLongLong();
        ledger.setRecurringEnabled(ruleId, false);
        const QVariantMap paused = ledger.generateRecurring();
        expect(QStringLiteral("暂停后不再生成"),
               !paused.value(QStringLiteral("ok")).toBool()
                   && transactions.countAll(&error) == before + 1,
               paused.value(QStringLiteral("message")).toString());

        ledger.removeRecurring(ruleId);
        expect(QStringLiteral("删除规则"), ledger.recurringRules().isEmpty());
    }
    // ---- 多账本：默认账本 / 新建切换 / 数据隔离 / 删除约束 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        expect(QStringLiteral("默认个人账本"),
               ledger.currentLedgerName() == QStringLiteral("个人账本")
                   && ledger.ledgers().size() >= 1,
               ledger.currentLedgerName());

        const QString defaultExpense = ledger.expenseText();
        const QString defaultNetWorth = ledger.netWorthText();

        const QVariantMap added = ledger.addLedger(QStringLiteral("家庭账本"));
        expect(QStringLiteral("新建「家庭账本」并自动切换"),
               added.value(QStringLiteral("ok")).toBool()
                   && ledger.currentLedgerName() == QStringLiteral("家庭账本")
                   && ledger.ledgers().size() == 2,
               added.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("新账本初始无流水"),
               ledger.expenseText() == QStringLiteral("¥0.00") && ledger.transactionCount() == 0,
               ledger.expenseText() + QStringLiteral(" / ") + QString::number(ledger.transactionCount()));

        const QVariantMap account = ledger.addAccount(QStringLiteral("家庭现金"),
                                                      QStringLiteral("cash"),
                                                      QStringLiteral("500.00"), QString());
        expect(QStringLiteral("新账本下建账户"), account.value(QStringLiteral("ok")).toBool(),
               account.value(QStringLiteral("message")).toString());

        const int familyAccountId = ledger.defaultAccountId();
        ledger.addTransaction(QStringLiteral("expense"), QStringLiteral("120.00"), familyAccountId,
                              ledger.defaultCategoryId(QStringLiteral("expense")),
                              QStringLiteral("2026-11-16"), QStringLiteral("家庭账本测试"));
        expect(QStringLiteral("家庭账本独立统计（支出 ¥120 / 净资产 ¥380）"),
               ledger.expenseText() == QStringLiteral("¥120.00")
                   && ledger.netWorthText() == QStringLiteral("¥380.00"),
               ledger.expenseText() + QStringLiteral(" / ") + ledger.netWorthText());

        // 跨账本允许同名账户（多账本核心诉求：家庭账本里也可以有「支付宝」）
        const QVariantMap familySameName = ledger.addAccount(QStringLiteral("支付宝"),
                                                             QStringLiteral("ewallet"),
                                                             QStringLiteral("800.00"), QString());
        expect(QStringLiteral("跨账本允许同名账户（家庭账本内也能叫「支付宝」）"),
               familySameName.value(QStringLiteral("ok")).toBool(),
               familySameName.value(QStringLiteral("message")).toString());

        const QVariantMap duplicateName = ledger.addAccount(QStringLiteral("支付宝"),
                                                            QStringLiteral("ewallet"),
                                                            QStringLiteral("1.00"), QString());
        expect(QStringLiteral("同账本重名被友好拒绝"),
               !duplicateName.value(QStringLiteral("ok")).toBool()
                   && duplicateName.value(QStringLiteral("message")).toString()
                          .contains(QStringLiteral("同名账户")),
               duplicateName.value(QStringLiteral("message")).toString());

        int defaultLedgerId = -1;
        int familyLedgerId = -1;
        for (const QVariant &item : ledger.ledgers()) {
            const QVariantMap row = item.toMap();
            if (row.value(QStringLiteral("isDefault")).toBool())
                defaultLedgerId = row.value(QStringLiteral("id")).toInt();
            if (row.value(QStringLiteral("name")).toString() == QStringLiteral("家庭账本"))
                familyLedgerId = row.value(QStringLiteral("id")).toInt();
        }

        ledger.switchLedger(defaultLedgerId);
        expect(QStringLiteral("切回个人账本数据不变"),
               ledger.expenseText() == defaultExpense && ledger.netWorthText() == defaultNetWorth,
               ledger.expenseText() + QStringLiteral(" / ") + ledger.netWorthText());

        expect(QStringLiteral("有账户的账本不可删除"),
               !ledger.removeLedger(familyLedgerId).value(QStringLiteral("ok")).toBool());
        expect(QStringLiteral("默认账本不可删除"),
               !ledger.removeLedger(defaultLedgerId).value(QStringLiteral("ok")).toBool());

        // 复原：把家庭账本里的流水与账户留在临时库中即可（自检库结束后整体删除）
        ledger.switchLedger(defaultLedgerId);
    }
    // ---- 数据安全：备份 / 全量导出 / 分类管理 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const QString dbPath = database.path();

        const int backupBefore = keep::BackupService::backupCount(dbPath);
        const QVariantMap backup = ledger.backupNow();
        const int backupAfter = keep::BackupService::backupCount(dbPath);
        expect(QStringLiteral("立即备份生成一份快照"),
               backup.value(QStringLiteral("ok")).toBool() && backupAfter == backupBefore + 1,
               backup.value(QStringLiteral("message")).toString());
        expect(QStringLiteral("备份时间可读"),
               ledger.lastBackupText() != QStringLiteral("从未备份"), ledger.lastBackupText());

        const QString exportDir = QDir(QDir::tempPath()).filePath(QStringLiteral("keep_export_test"));
        QDir(exportDir).removeRecursively();
        QDir().mkpath(exportDir);
        const QVariantMap exported = ledger.exportAllJson(exportDir);
        bool jsonValid = false;
        int exportTxCount = -1;
        int exportAccountCount = -1;
        if (exported.value(QStringLiteral("ok")).toBool()) {
            QFile jsonFile(exported.value(QStringLiteral("path")).toString());
            if (jsonFile.open(QIODevice::ReadOnly)) {
                const QJsonDocument document = QJsonDocument::fromJson(jsonFile.readAll());
                jsonFile.close();
                jsonValid = document.isObject();
                const QJsonObject data = document.object().value(QStringLiteral("data")).toObject();
                exportTxCount = data.value(QStringLiteral("transactions")).toArray().size();
                exportAccountCount = data.value(QStringLiteral("accounts")).toArray().size();
            }
        }
        expect(QStringLiteral("全量 JSON 导出（表结构 + 行数与库一致）"),
               jsonValid && exportTxCount == transactions.countAll(&error)
                   && exportAccountCount
                          == static_cast<int>(accounts.listWithBalances(true).size()),
               QStringLiteral("流水 %1 / 账户 %2")
                   .arg(exportTxCount)
                   .arg(exportAccountCount));
        QDir(exportDir).removeRecursively();

        const int categoriesBefore = ledger.expenseCategories().size();
        const QVariantMap addedCategory =
            ledger.addCategory(QStringLiteral("expense"), QStringLiteral("咖啡自检"),
                               QStringLiteral("#2563EB"));
        expect(QStringLiteral("新增分类"),
               addedCategory.value(QStringLiteral("ok")).toBool()
                   && ledger.expenseCategories().size() == categoriesBefore + 1,
               addedCategory.value(QStringLiteral("message")).toString());

        const QVariantMap duplicateCategory =
            ledger.addCategory(QStringLiteral("expense"), QStringLiteral("咖啡自检"), QString());
        expect(QStringLiteral("同类型重名分类被拒绝"),
               !duplicateCategory.value(QStringLiteral("ok")).toBool(),
               duplicateCategory.value(QStringLiteral("message")).toString());

        int coffeeCategoryId = -1;
        for (const QVariant &item : ledger.managedCategories()) {
            const QVariantMap row = item.toMap();
            if (row.value(QStringLiteral("name")).toString() == QStringLiteral("咖啡自检"))
                coffeeCategoryId = row.value(QStringLiteral("id")).toInt();
        }
        const QVariantMap renamedCategory =
            ledger.updateCategory(coffeeCategoryId, QStringLiteral("咖啡续杯"),
                                  QStringLiteral("#F97316"));
        expect(QStringLiteral("分类改名与改色"), renamedCategory.value(QStringLiteral("ok")).toBool(),
               renamedCategory.value(QStringLiteral("message")).toString());

        const QVariantMap archivedCategory = ledger.archiveCategory(coffeeCategoryId, true);
        expect(QStringLiteral("归档后不再出现在记账选择里"),
               archivedCategory.value(QStringLiteral("ok")).toBool()
                   && ledger.expenseCategories().size() == categoriesBefore,
               archivedCategory.value(QStringLiteral("message")).toString());

        // 清理自检产生的备份快照，避免临时目录堆积
        QDir(keep::BackupService::backupDirectory(dbPath)).removeRecursively();
    }
    // ---- A 数据层：小类 / 转账口径 / 应收应付 / 时间到分钟 ----
    {
        keep::LedgerViewModel ledger(database.handle());
        const auto centsOf = [](const QString &text) -> qint64 {
            QString clean = text;
            clean.remove(QChar(0x00A5));
            clean.remove(QChar(0xFFE5));
            clean.remove(QLatin1Char(','));
            const auto money = keep::Money::fromYuan(clean);
            return money.has_value() ? money->cents() : 0;
        };

        // 小类播种（餐饮下应有多个小类）
        int diningId = -1;
        QStringList diningSubs;
        int childCount = 0;
        for (const QVariant &big : ledger.expenseCategoryTree()) {
            const QVariantMap row = big.toMap();
            const QVariantList children = row.value(QStringLiteral("children")).toList();
            childCount += children.size();
            if (row.value(QStringLiteral("name")).toString() == QStringLiteral("餐饮")) {
                diningId = row.value(QStringLiteral("id")).toInt();
                for (const QVariant &child : children)
                    diningSubs << child.toMap().value(QStringLiteral("name")).toString();
            }
        }
        expect(QStringLiteral("大类下已播种小类（餐饮 ≥ 6 个，含「午餐」）"),
               diningSubs.size() >= 6 && diningSubs.contains(QStringLiteral("午餐")),
               diningSubs.join(QStringLiteral(",")));

        // 幂等：再次构造 VM 不会重复播种
        {
            keep::LedgerViewModel again(database.handle());
            int childCountAgain = 0;
            for (const QVariant &big : again.expenseCategoryTree())
                childCountAgain += big.toMap().value(QStringLiteral("children")).toList().size();
            expect(QStringLiteral("小类播种幂等（重复启动不翻倍）"), childCountAgain == childCount,
                   QStringLiteral("%1 -> %2").arg(childCount).arg(childCountAgain));
        }

        const int bankId = ledger.defaultAccountId();
        // 取同一账本内的另一个账户作为转入方（跨账本互转会被拒绝）
        keep::AccountRepository scopedAccounts(database.handle());
        scopedAccounts.setLedgerScope(ledger.currentLedgerId());
        int walletId = -1;
        for (const keep::AccountWithBalance &row : scopedAccounts.listWithBalances()) {
            if (row.account.id != bankId) {
                walletId = static_cast<int>(row.account.id);
                break;
            }
        }

        // 时间精确到分钟
        QVariantMap entry;
        entry.insert(QStringLiteral("kind"), QStringLiteral("expense"));
        entry.insert(QStringLiteral("amountText"), QStringLiteral("18.00"));
        entry.insert(QStringLiteral("accountId"), bankId);
        entry.insert(QStringLiteral("categoryId"), diningId);
        entry.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-13"));
        entry.insert(QStringLiteral("timeText"), QStringLiteral("18:45"));
        entry.insert(QStringLiteral("note"), QStringLiteral("时间自检"));
        const QVariantMap saved = ledger.saveEntry(entry);
        expect(QStringLiteral("saveEntry 保存成功"), saved.value(QStringLiteral("ok")).toBool(),
               saved.value(QStringLiteral("message")).toString());

        bool minuteOk = false;
        for (const keep::Transaction &row : transactions.listByMonth(2026, 11)) {
            if (row.note == QStringLiteral("时间自检"))
                minuteOk = row.occurredAt.time() == QTime(18, 45) && row.occurredAt.time().second() == 0;
        }
        expect(QStringLiteral("时间保存为 18:45（秒固定 00）"), minuteOk);

        // 账户互转：不计收支、不动净资产
        const qint64 expenseBefore = centsOf(ledger.expenseText());
        const qint64 netBefore = centsOf(ledger.netWorthText());

        QVariantMap transfer;
        transfer.insert(QStringLiteral("kind"), QStringLiteral("transfer"));
        transfer.insert(QStringLiteral("amountText"), QStringLiteral("500.00"));
        transfer.insert(QStringLiteral("accountId"), bankId);
        transfer.insert(QStringLiteral("toAccountId"), walletId);
        transfer.insert(QStringLiteral("dateText"), QStringLiteral("2026-11-13"));
        transfer.insert(QStringLiteral("timeText"), QStringLiteral("12:00"));
        transfer.insert(QStringLiteral("transferNature"), QStringLiteral("transfer"));
        const QVariantMap moved = ledger.saveEntry(transfer);
        expect(QStringLiteral("账户互转保存成功并且不计入支出"),
               moved.value(QStringLiteral("ok")).toBool()
                   && centsOf(ledger.expenseText()) == expenseBefore
                   && centsOf(ledger.netWorthText()) == netBefore,
               ledger.expenseText() + QStringLiteral(" / ") + ledger.netWorthText());

        // 应收账户：借出后净资产不变（现金 → 应收）
        const QVariantMap receivable = ledger.addAccount(QStringLiteral("应收 · 张三"),
                                                         QStringLiteral("receivable"),
                                                         QStringLiteral("0.00"), QString());
        expect(QStringLiteral("创建应收账户"), receivable.value(QStringLiteral("ok")).toBool(),
               receivable.value(QStringLiteral("message")).toString());
        const int receivableId = receivable.value(QStringLiteral("id")).toInt();

        QVariantMap lend = transfer;
        lend.insert(QStringLiteral("amountText"), QStringLiteral("300.00"));
        lend.insert(QStringLiteral("toAccountId"), receivableId);
        lend.insert(QStringLiteral("transferNature"), QStringLiteral("lend"));
        const QVariantMap lent = ledger.saveEntry(lend);
        expect(QStringLiteral("借出：净资产不变、支出不变"),
               lent.value(QStringLiteral("ok")).toBool()
                   && centsOf(ledger.expenseText()) == expenseBefore
                   && centsOf(ledger.netWorthText()) == netBefore,
               ledger.netWorthText());

        // 转给他人 · 记作支出（默认口径）
        QVariantMap asExpense = transfer;
        asExpense.insert(QStringLiteral("amountText"), QStringLiteral("100.00"));
        asExpense.insert(QStringLiteral("toAccountId"), -1);
        asExpense.insert(QStringLiteral("transferNature"), QStringLiteral("expense"));
        const QVariantMap expensed = ledger.saveEntry(asExpense);
        expect(QStringLiteral("转给他人记作支出：本月支出 +¥100"),
               expensed.value(QStringLiteral("ok")).toBool()
                   && centsOf(ledger.expenseText()) == expenseBefore + 10000,
               ledger.expenseText());

        // 应付账户：计入负债
        const QVariantMap payable = ledger.addAccount(QStringLiteral("应付 · 李四"),
                                                      QStringLiteral("payable"),
                                                      QStringLiteral("200.00"), QString());
        // 此时净资产 = 基线 − 记作支出的 ¥100 − 应付负债 ¥200
        expect(QStringLiteral("应付账户计入负债：净资产再 −¥200"),
               payable.value(QStringLiteral("ok")).toBool()
                   && centsOf(ledger.netWorthText()) == netBefore - 10000 - 20000,
               ledger.netWorthText());

        ledger.saveThemeDark(false);
    }

    keep::BudgetRepository budgetRepository(database.handle());
    const auto budget = budgetRepository.monthlyLimit(QStringLiteral("2026-11"), &error);
    expect(QStringLiteral("11 月预算 ¥6,500.00"), budget.has_value() && budget->cents() == 650000,
           budget ? budget->format() : QStringLiteral("未设置"));

    const auto parsed = keep::Money::fromYuan(QStringLiteral("1,299.00"));
    expect(QStringLiteral("金额解析 \"1,299.00\""), parsed.has_value() && parsed->cents() == 129900,
           parsed ? parsed->format() : QStringLiteral("解析失败"));
    expect(QStringLiteral("金额格式化 590790"), keep::Money::fromCents(590790).format() == QStringLiteral("¥5,907.90"),
           keep::Money::fromCents(590790).format());
    expect(QStringLiteral("负数格式化 -218450"),
           keep::Money::fromCents(-218450).format() == QStringLiteral("-¥2,184.50"),
           keep::Money::fromCents(-218450).format());

    }  // 迁移器与仓储都在这里销毁，再关闭数据库，避免 "connection is still in use"

    database.close();
    for (const QString &suffix : {QString(), QStringLiteral("-wal"), QStringLiteral("-shm")})
        QFile::remove(path + suffix);

    if (g_failures == 0) {
        qInfo().noquote() << QStringLiteral("自检全部通过");
        return 0;
    }
    qCritical().noquote() << QStringLiteral("自检失败 %1 项").arg(g_failures);
    return 1;
}

}  // namespace

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("moyu_ledger"));
    QCoreApplication::setApplicationName(QStringLiteral("摸鱼记账"));

#ifdef MOYU_LEDGER_VERSION
    QCoreApplication::setApplicationVersion(QStringLiteral(MOYU_LEDGER_VERSION));
#else
    QCoreApplication::setApplicationVersion(QStringLiteral("1.0.0"));
#endif

    const QStringList arguments = app.arguments();

    if (arguments.contains(QStringLiteral("--self-test")))
        return runSelfTest();

    const bool selfCheck = arguments.contains(QStringLiteral("--db-check"));
    const bool seedDemo = arguments.contains(QStringLiteral("--seed-demo"));

    keep::Database database;
    QString error;

    // 数据安全：升级 schema 前先留一份快照（仅当版本落后时才备份）
    const QString dbPath = keep::Database::defaultPath();
    {
        QString backupError;
        if (!keep::BackupService::snapshotBeforeUpgrade(dbPath, &backupError)) {
            qWarning().noquote() << QStringLiteral("迁移前备份失败（继续启动）：%1").arg(backupError);
        }
    }

    if (!database.open(dbPath, &error)) {
        qCritical().noquote() << QStringLiteral("数据库打开失败：%1").arg(error);
        return 2;
    }

    keep::Migrator migrator(database.handle());
    if (!migrator.migrate(&error)) {
        qCritical().noquote() << QStringLiteral("数据库迁移失败：%1").arg(error);
        return 3;
    }

    if (seedDemo) {
        if (!keep::DemoSeeder::seed(database.handle(), &error)) {
            qCritical().noquote() << QStringLiteral("演示数据写入失败：%1").arg(error);
            return 5;
        }
    }

    qInfo().noquote() << QStringLiteral("数据库就绪：%1（schema v%2，%3 张表）")
                             .arg(database.path())
                             .arg(migrator.currentVersion())
                             .arg(countUserTables(database.handle()));

    if (seedDemo) {
        qInfo().noquote() << QStringLiteral("演示数据已写入，再次启动即可查看");
        return 0;
    }

    if (selfCheck) {
        const bool fkOn = foreignKeysEnforced(database.handle());
        qInfo().noquote() << QStringLiteral("foreign_keys 强制生效：%1")
                                 .arg(fkOn ? QStringLiteral("是") : QStringLiteral("否"));
        return fkOn ? 0 : 4;
    }

    // 每天首次启动自动备份一次（保留最近 7 份）
    {
        QString backupError;
        if (!keep::BackupService::ensureDailyBackup(database.handle(), database.path(), 24,
                                                    &backupError)) {
            qWarning().noquote() << QStringLiteral("自动备份失败：%1").arg(backupError);
        }
    }

    keep::LedgerViewModel ledger(database.handle());

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("Ledger"), &ledger);
    // qt_add_qml_module 生成的资源路径为 qrc:/qt/qml/<URI>/<入口QML>
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/MoyuLedger/Main.qml")));

    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}