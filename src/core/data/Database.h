#pragma once

#include <QSqlDatabase>
#include <QString>

namespace keep {

// 单进程 SQLite 连接管理：建目录、开 WAL、保留固定连接名，便于测试替换路径。
class Database
{
public:
    Database();
    ~Database();

    Database(const Database &) = delete;
    Database &operator=(const Database &) = delete;

    // 默认库路径：%LOCALAPPDATA%/MoyuLedger/data.db
    // 可用环境变量 MOYU_LEDGER_DB（兼容旧的 KEEP_ACCOUNT_DB） 覆盖（自检与将来测试用）
    [[nodiscard]] static QString defaultPath();

    [[nodiscard]] bool open(const QString &path, QString *error = nullptr);
    void close();

    [[nodiscard]] bool isOpen() const { return m_db.isOpen(); }
    [[nodiscard]] QSqlDatabase handle() const { return m_db; }
    [[nodiscard]] QString path() const { return m_path; }

private:
    QSqlDatabase m_db;
    QString m_path;
    QString m_connectionName;
};

}  // namespace keep