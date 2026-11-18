#include "core/data/Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QtGlobal>

namespace keep {

namespace {
constexpr auto kConnectionName = "keep_main";
constexpr auto kFileName = "data.db";
}  // namespace

Database::Database()
    : m_connectionName(QString::fromLatin1(kConnectionName))
{
}

Database::~Database()
{
    close();
}

QString Database::defaultPath()
{
    // 单文件库路径（自检/测试用）
    const QString overridePath = qEnvironmentVariable("MOYU_LEDGER_DB", qEnvironmentVariable("KEEP_ACCOUNT_DB"));
    if (!overridePath.isEmpty())
        return overridePath;

    // 数据目录覆盖（便携版 launcher 用：独立目录 → 开箱即空的干净账本）
    const QString homeOverride = qEnvironmentVariable("MOYU_LEDGER_HOME", qEnvironmentVariable("KEEP_ACCOUNT_HOME"));
    if (!homeOverride.isEmpty())
        return QDir(homeOverride).filePath(QString::fromLatin1(kFileName));

    const QString base = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
    // 改名迁移：旧的 %LOCALAPPDATA%/KeepAccount 目录整体搬成 MoyuLedger，老用户数据不丢
    {
        const QString newHome = QDir(base).filePath(QStringLiteral("MoyuLedger"));
        const QString legacyHome = QDir(base).filePath(QStringLiteral("KeepAccount"));
        if (!QDir(newHome).exists() && QDir(legacyHome).exists()) {
            if (QDir().rename(legacyHome, newHome))
                qInfo("已将旧数据目录 KeepAccount 迁移为 MoyuLedger");
        }
    }
    return QDir(base).filePath(QStringLiteral("MoyuLedger/%1").arg(QString::fromLatin1(kFileName)));
}

bool Database::open(const QString &path, QString *error)
{
    close();

    if (!QSqlDatabase::isDriverAvailable(QStringLiteral("QSQLITE"))) {
        if (error)
            *error = QStringLiteral("QSQLITE 驱动不可用");
        return false;
    }

    const QFileInfo info(path);
    if (!QDir().mkpath(info.absolutePath())) {
        if (error)
            *error = QStringLiteral("无法创建数据目录：%1").arg(info.absolutePath());
        return false;
    }

    m_path = path;
    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(path);

    if (!m_db.open()) {
        if (error)
            *error = m_db.lastError().text();
        close();
        return false;
    }

    // 每个连接都必须重新设置：foreign_keys 是连接级开关
    const QStringList pragmas{
        QStringLiteral("PRAGMA journal_mode = WAL"),
        QStringLiteral("PRAGMA synchronous = NORMAL"),
        QStringLiteral("PRAGMA foreign_keys = ON"),
        QStringLiteral("PRAGMA busy_timeout = 5000"),
    };

    QSqlQuery query(m_db);
    for (const QString &sql : pragmas) {
        if (!query.exec(sql)) {
            if (error)
                *error = QStringLiteral("%1 失败：%2").arg(sql, query.lastError().text());
            close();
            return false;
        }
    }

    return true;
}

void Database::close()
{
    if (m_db.isValid()) {
        if (m_db.isOpen())
            m_db.close();
        m_db = QSqlDatabase();
    }

    if (QSqlDatabase::contains(m_connectionName))
        QSqlDatabase::removeDatabase(m_connectionName);

    m_path.clear();
}

}  // namespace keep