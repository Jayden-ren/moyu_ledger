#pragma once

#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

namespace keep {

struct Migration
{
    int version;
    QString name;
    QStringList statements;  // 逐条执行，避免按分号切分 SQL
    // 需要重建表（例如修改 UNIQUE 约束）时临时关闭外键，迁移结束后自动恢复并校验
    bool requireForeignKeysOff = false;
};

// 增量迁移：只追加、不改历史；每个版本在一个事务里执行，失败整体回滚。
class Migrator
{
public:
    explicit Migrator(QSqlDatabase db);

    [[nodiscard]] static const QVector<Migration> &migrations();

    [[nodiscard]] int currentVersion(QString *error = nullptr) const;
    [[nodiscard]] bool migrate(QString *error = nullptr);

private:
    [[nodiscard]] bool ensureVersionTable(QString *error);

    QSqlDatabase m_db;
};

}  // namespace keep