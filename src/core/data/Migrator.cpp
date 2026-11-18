#include "core/data/Migrator.h"

#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace keep {

namespace {
QString sql(const char *statement)
{
    return QString::fromUtf8(statement);
}
}  // namespace

Migrator::Migrator(QSqlDatabase db)
    : m_db(std::move(db))
{
}

const QVector<Migration> &Migrator::migrations()
{
    static const QVector<Migration> steps = {
        Migration{
            .version = 1,
            .name = QStringLiteral("初始表结构"),
            .statements = {
                sql(R"sql(
CREATE TABLE accounts (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT    NOT NULL UNIQUE,
    type                  TEXT    NOT NULL CHECK (type IN
                              ('savings','checking','credit','ewallet','cash','investment')),
    currency              TEXT    NOT NULL DEFAULT 'CNY',
    opening_balance_cents INTEGER NOT NULL DEFAULT 0,
    credit_limit_cents    INTEGER,
    icon                  TEXT,
    color                 TEXT,
    sort_order            INTEGER NOT NULL DEFAULT 0,
    is_archived           INTEGER NOT NULL DEFAULT 0,
    created_at            TEXT    NOT NULL
))sql"),
                sql(R"sql(
CREATE TABLE categories (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    parent_id   INTEGER REFERENCES categories(id) ON DELETE RESTRICT,
    name        TEXT    NOT NULL,
    kind        TEXT    NOT NULL CHECK (kind IN ('expense','income')),
    icon        TEXT,
    color       TEXT,
    sort_order  INTEGER NOT NULL DEFAULT 0,
    is_builtin  INTEGER NOT NULL DEFAULT 0,
    is_archived INTEGER NOT NULL DEFAULT 0,
    UNIQUE (parent_id, name, kind)
))sql"),
                sql(R"sql(
CREATE TABLE transactions (
    id            INTEGER PRIMARY KEY AUTOINCREMENT,
    kind          TEXT    NOT NULL CHECK (kind IN ('expense','income','transfer')),
    amount_cents  INTEGER NOT NULL CHECK (amount_cents > 0),
    account_id    INTEGER NOT NULL REFERENCES accounts(id) ON DELETE RESTRICT,
    to_account_id INTEGER REFERENCES accounts(id) ON DELETE RESTRICT,
    category_id   INTEGER REFERENCES categories(id) ON DELETE RESTRICT,
    occurred_at   TEXT    NOT NULL,
    note          TEXT    NOT NULL DEFAULT '',
    payee         TEXT    NOT NULL DEFAULT '',
    external_id   TEXT,
    created_at    TEXT    NOT NULL,
    updated_at    TEXT    NOT NULL,
    CHECK (kind <> 'transfer' OR to_account_id IS NOT NULL)
))sql"),
                sql("CREATE INDEX idx_tx_occurred ON transactions(occurred_at DESC)"),
                sql("CREATE INDEX idx_tx_account  ON transactions(account_id, occurred_at DESC)"),
                sql("CREATE INDEX idx_tx_category ON transactions(category_id, occurred_at DESC)"),
                sql(R"sql(
CREATE TABLE tags (
    id    INTEGER PRIMARY KEY AUTOINCREMENT,
    name  TEXT NOT NULL UNIQUE COLLATE NOCASE,
    color TEXT
))sql"),
                sql(R"sql(
CREATE TABLE transaction_tags (
    transaction_id INTEGER NOT NULL REFERENCES transactions(id) ON DELETE CASCADE,
    tag_id         INTEGER NOT NULL REFERENCES tags(id) ON DELETE CASCADE,
    PRIMARY KEY (transaction_id, tag_id)
))sql"),
                sql(R"sql(
CREATE TABLE budgets (
    id           INTEGER PRIMARY KEY AUTOINCREMENT,
    year_month   TEXT    NOT NULL,
    category_id  INTEGER REFERENCES categories(id) ON DELETE CASCADE,
    amount_cents INTEGER NOT NULL CHECK (amount_cents >= 0),
    rollover     INTEGER NOT NULL DEFAULT 0,
    UNIQUE (year_month, category_id)
))sql"),
                sql(R"sql(
CREATE TABLE settings (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
))sql"),
            },
        },
        // ---- V2：定期账单 ----
        {
            2,
            QStringLiteral("recurring bills"),
            {
                sql(R"sql(
CREATE TABLE recurring_rules (
    id                   INTEGER PRIMARY KEY AUTOINCREMENT,
    kind                 TEXT    NOT NULL CHECK (kind IN ('income', 'expense')),
    amount_cents         INTEGER NOT NULL CHECK (amount_cents > 0),
    account_id           INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    category_id          INTEGER REFERENCES categories(id) ON DELETE SET NULL,
    note                 TEXT    NOT NULL DEFAULT '',
    day_of_month         INTEGER NOT NULL CHECK (day_of_month BETWEEN 1 AND 28),
    enabled              INTEGER NOT NULL DEFAULT 1,
    last_generated_month TEXT,
    created_at           TEXT    NOT NULL
))sql"),
                sql(R"sql(CREATE INDEX idx_recurring_enabled ON recurring_rules(enabled))sql"),
            },
        },
        // ---- V3：多账本 ----
        {
            3,
            QStringLiteral("multi ledger"),
            {
                sql(R"sql(
CREATE TABLE ledgers (
    id         INTEGER PRIMARY KEY AUTOINCREMENT,
    name       TEXT    NOT NULL UNIQUE COLLATE NOCASE,
    color      TEXT,
    is_default INTEGER NOT NULL DEFAULT 0,
    sort_order INTEGER NOT NULL DEFAULT 0,
    created_at TEXT    NOT NULL
))sql"),
                sql(R"sql(
INSERT INTO ledgers(name, color, is_default, sort_order, created_at)
VALUES ('个人账本', '#2563EB', 1, 0, datetime('now', 'localtime'))
)sql"),
                sql(R"sql(
ALTER TABLE accounts ADD COLUMN ledger_id INTEGER REFERENCES ledgers(id) ON DELETE SET NULL
)sql"),
                sql(R"sql(
UPDATE accounts SET ledger_id = (SELECT id FROM ledgers WHERE is_default = 1)
 WHERE ledger_id IS NULL
)sql"),
                sql(R"sql(CREATE INDEX idx_accounts_ledger ON accounts(ledger_id))sql"),
            },
        },
        // ---- V4：账户名改成「账本内唯一」（多账本下允许不同账本同名） ----
        {
            4,
            QStringLiteral("account name unique per ledger"),
            {
                sql(R"sql(
CREATE TABLE accounts_new (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT    NOT NULL,
    type                  TEXT    NOT NULL CHECK (type IN
                              ('savings','checking','credit','ewallet','cash','investment')),
    currency              TEXT    NOT NULL DEFAULT 'CNY',
    opening_balance_cents INTEGER NOT NULL DEFAULT 0,
    credit_limit_cents    INTEGER,
    icon                  TEXT,
    color                 TEXT,
    sort_order            INTEGER NOT NULL DEFAULT 0,
    is_archived           INTEGER NOT NULL DEFAULT 0,
    created_at            TEXT    NOT NULL,
    ledger_id             INTEGER REFERENCES ledgers(id) ON DELETE SET NULL,
    UNIQUE (ledger_id, name)
))sql"),
                sql(R"sql(
INSERT INTO accounts_new(id, name, type, currency, opening_balance_cents, credit_limit_cents,
                         icon, color, sort_order, is_archived, created_at, ledger_id)
SELECT id, name, type, currency, opening_balance_cents, credit_limit_cents,
       icon, color, sort_order, is_archived, created_at,
       COALESCE(ledger_id, (SELECT id FROM ledgers WHERE is_default = 1))
  FROM accounts
)sql"),
                sql(R"sql(DROP TABLE accounts)sql"),
                sql(R"sql(ALTER TABLE accounts_new RENAME TO accounts)sql"),
                sql(R"sql(CREATE INDEX idx_accounts_ledger ON accounts(ledger_id))sql"),
            },
            true,  // 重建表需要临时关闭外键
        },
        // ---- V5：账户类型新增 应收 / 应付（往来账户） ----
        {
            5,
            QStringLiteral("receivable and payable accounts"),
            {
                sql(R"sql(
CREATE TABLE accounts_new (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT    NOT NULL,
    type                  TEXT    NOT NULL CHECK (type IN
                              ('savings','checking','credit','ewallet','cash','investment',
                               'receivable','payable')),
    currency              TEXT    NOT NULL DEFAULT 'CNY',
    opening_balance_cents INTEGER NOT NULL DEFAULT 0,
    credit_limit_cents    INTEGER,
    icon                  TEXT,
    color                 TEXT,
    sort_order            INTEGER NOT NULL DEFAULT 0,
    is_archived           INTEGER NOT NULL DEFAULT 0,
    created_at            TEXT    NOT NULL,
    ledger_id             INTEGER REFERENCES ledgers(id) ON DELETE SET NULL,
    UNIQUE (ledger_id, name)
))sql"),
                sql(R"sql(
INSERT INTO accounts_new(id, name, type, currency, opening_balance_cents, credit_limit_cents,
                         icon, color, sort_order, is_archived, created_at, ledger_id)
SELECT id, name, type, currency, opening_balance_cents, credit_limit_cents,
       icon, color, sort_order, is_archived, created_at, ledger_id
  FROM accounts
)sql"),
                sql(R"sql(DROP TABLE accounts)sql"),
                sql(R"sql(ALTER TABLE accounts_new RENAME TO accounts)sql"),
                sql(R"sql(CREATE INDEX idx_accounts_ledger ON accounts(ledger_id))sql"),
            },
            true,
        },
        // ---- V6：理财 / 股票账户 + 每日收益（含佣金、市值）记录 ----
        {
            6,
            QStringLiteral("wealth and stock accounts with daily income records"),
            {
                sql(R"sql(
CREATE TABLE accounts_new (
    id                    INTEGER PRIMARY KEY AUTOINCREMENT,
    name                  TEXT    NOT NULL,
    type                  TEXT    NOT NULL CHECK (type IN
                              ('savings','checking','credit','ewallet','cash','investment',
                               'receivable','payable','wealth','stock')),
    currency              TEXT    NOT NULL DEFAULT 'CNY',
    opening_balance_cents INTEGER NOT NULL DEFAULT 0,
    credit_limit_cents    INTEGER,
    icon                  TEXT,
    color                 TEXT,
    sort_order            INTEGER NOT NULL DEFAULT 0,
    is_archived           INTEGER NOT NULL DEFAULT 0,
    created_at            TEXT    NOT NULL,
    ledger_id             INTEGER REFERENCES ledgers(id) ON DELETE SET NULL,
    UNIQUE (ledger_id, name)
))sql"),
                sql(R"sql(
INSERT INTO accounts_new(id, name, type, currency, opening_balance_cents, credit_limit_cents,
                         icon, color, sort_order, is_archived, created_at, ledger_id)
SELECT id, name, type, currency, opening_balance_cents, credit_limit_cents,
       icon, color, sort_order, is_archived, created_at, ledger_id
  FROM accounts
)sql"),
                sql(R"sql(DROP TABLE accounts)sql"),
                sql(R"sql(ALTER TABLE accounts_new RENAME TO accounts)sql"),
                sql(R"sql(CREATE INDEX idx_accounts_ledger ON accounts(ledger_id))sql"),
                // 每日收益记录：每个账户每天一条；收益/佣金会联动生成流水
                sql(R"sql(
CREATE TABLE account_daily_records (
    id                  INTEGER PRIMARY KEY AUTOINCREMENT,
    account_id          INTEGER NOT NULL REFERENCES accounts(id) ON DELETE CASCADE,
    occurred_on         TEXT    NOT NULL,
    income_cents        INTEGER NOT NULL DEFAULT 0,
    commission_cents    INTEGER NOT NULL DEFAULT 0,
    market_value_cents  INTEGER,
    note                TEXT,
    income_tx_id        INTEGER REFERENCES transactions(id) ON DELETE SET NULL,
    commission_tx_id    INTEGER REFERENCES transactions(id) ON DELETE SET NULL,
    created_at          TEXT    NOT NULL,
    updated_at          TEXT    NOT NULL,
    UNIQUE (account_id, occurred_on)
))sql"),
                sql(R"sql(CREATE INDEX idx_daily_records_account ON account_daily_records(account_id, occurred_on))sql"),
            },
            true,
        },
    };
    return steps;
}

bool Migrator::ensureVersionTable(QString *error)
{
    QSqlQuery query(m_db);
    const bool ok = query.exec(sql(R"sql(
CREATE TABLE IF NOT EXISTS schema_version (
    version    INTEGER PRIMARY KEY,
    applied_at TEXT    NOT NULL
))sql"));

    if (!ok && error)
        *error = query.lastError().text();

    return ok;
}

int Migrator::currentVersion(QString *error) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) || !query.next()) {
        if (error)
            *error = query.lastError().text();
        return -1;
    }

    const QVariant value = query.value(0);
    return value.isNull() ? 0 : value.toInt();
}

bool Migrator::migrate(QString *error)
{
    if (!ensureVersionTable(error))
        return false;

    int version = currentVersion(error);
    if (version < 0)
        return false;

    for (const Migration &step : migrations()) {
        if (step.version <= version)
            continue;

        // 重建表类迁移需要先关掉外键（PRAGMA 不能在事务内修改）
        if (step.requireForeignKeysOff) {
            QSqlQuery pragma(m_db);
            pragma.exec(QStringLiteral("PRAGMA foreign_keys = OFF"));
        }

        if (!m_db.transaction()) {
            if (error)
                *error = m_db.lastError().text();
            return false;
        }

        QSqlQuery query(m_db);
        for (const QString &statement : step.statements) {
            if (!query.exec(statement)) {
                if (error) {
                    *error = QStringLiteral("迁移 v%1（%2）失败：%3")
                                 .arg(step.version)
                                 .arg(step.name, query.lastError().text());
                }
                m_db.rollback();
                return false;
            }
        }

        QSqlQuery record(m_db);
        record.prepare(QStringLiteral("INSERT INTO schema_version(version, applied_at) VALUES (?, ?)"));
        record.addBindValue(step.version);
        record.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

        if (!record.exec() || !m_db.commit()) {
            if (error)
                *error = record.lastError().text();
            m_db.rollback();
            if (step.requireForeignKeysOff) {
                QSqlQuery pragma(m_db);
                pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));
            }
            return false;
        }

        if (step.requireForeignKeysOff) {
            QSqlQuery pragma(m_db);
            pragma.exec(QStringLiteral("PRAGMA foreign_keys = ON"));

            // 恢复后做一次完整性校验，有问题就报错（不回滚已提交的迁移，但明确提示）
            QSqlQuery check(m_db);
            if (check.exec(QStringLiteral("PRAGMA foreign_key_check")) && check.next()) {
                if (error) {
                    *error = QStringLiteral("迁移 v%1 后外键校验失败：表 %2")
                                 .arg(step.version)
                                 .arg(check.value(0).toString());
                }
                return false;
            }
        }

        version = step.version;
    }

    return true;
}

}  // namespace keep