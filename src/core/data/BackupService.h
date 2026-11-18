#pragma once

#include <QDateTime>
#include <QSqlDatabase>
#include <QString>

namespace keep {

// 数据安全：数据库快照备份（VACUUM INTO 生成一致性副本）、按份数滚动、跨天自动备份
class BackupService
{
public:
    // <db目录>/backups
    [[nodiscard]] static QString backupDirectory(const QString &dbPath);

    // 立即生成一份快照；成功时 outPath 返回文件路径
    [[nodiscard]] static bool backupNow(QSqlDatabase db, const QString &dbPath,
                                        QString *outPath = nullptr, QString *error = nullptr);

    // 只保留最近 keep 份
    static void prune(const QString &dbPath, int keep = 7);

    [[nodiscard]] static QDateTime lastBackupTime(const QString &dbPath);
    [[nodiscard]] static int backupCount(const QString &dbPath);

    // 距离上次备份超过 minHours 小时就备份一次（启动时调用）
    [[nodiscard]] static bool ensureDailyBackup(QSqlDatabase db, const QString &dbPath,
                                                int minHours = 24, QString *error = nullptr);

    // 迁移前快照：仅在「数据库已存在且 schema 版本落后」时备份，避免每次启动多一份
    [[nodiscard]] static bool snapshotBeforeUpgrade(const QString &dbPath, QString *error = nullptr);

private:
    [[nodiscard]] static QStringList snapshotFiles(const QString &dbPath);
};

}  // namespace keep