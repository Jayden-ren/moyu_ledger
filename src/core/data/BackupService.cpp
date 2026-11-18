#include "core/data/BackupService.h"

#include "core/data/Migrator.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStringList>
#include <QVector>

namespace keep {

namespace {

constexpr auto kDirectoryName = "backups";
constexpr auto kFilePrefix = "data-";
constexpr auto kFileSuffix = ".db";

QString escapedLiteral(const QString &path)
{
    QString escaped = path;
    escaped.replace(QLatin1Char('\''), QStringLiteral("''"));
    return escaped;
}

}  // namespace

QString BackupService::backupDirectory(const QString &dbPath)
{
    const QFileInfo info(dbPath);
    return QDir(info.absolutePath()).filePath(QString::fromLatin1(kDirectoryName));
}

QStringList BackupService::snapshotFiles(const QString &dbPath)
{
    const QDir dir(backupDirectory(dbPath));
    if (!dir.exists())
        return {};

    QStringList files;
    for (const QString &name : dir.entryList({QStringLiteral("data-*.db")}, QDir::Files, QDir::Name))
        files.append(dir.filePath(name));
    // 文件名时间戳升序，最新的在末尾
    return files;
}

bool BackupService::backupNow(QSqlDatabase db, const QString &dbPath, QString *outPath,
                              QString *error)
{
    const QString dir = backupDirectory(dbPath);
    if (!QDir().mkpath(dir)) {
        if (error)
            *error = QStringLiteral("无法创建备份目录：%1").arg(dir);
        return false;
    }

    const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"));
    QString target = QDir(dir).filePath(QStringLiteral("%1%2%3")
                                            .arg(QString::fromLatin1(kFilePrefix), stamp,
                                                 QString::fromLatin1(kFileSuffix)));
    // VACUUM INTO 不允许目标已存在：同一毫秒内重复备份时追加序号
    for (int suffix = 2; QFileInfo::exists(target); ++suffix) {
        target = QDir(dir).filePath(QStringLiteral("%1%2-%3%4")
                                        .arg(QString::fromLatin1(kFilePrefix), stamp)
                                        .arg(suffix)
                                        .arg(QString::fromLatin1(kFileSuffix)));
        if (suffix > 99)
            break;
    }

    QSqlQuery query(db);
    // VACUUM INTO 生成一致性快照（WAL 下也安全），不需要停机
    const QString sql = QStringLiteral("VACUUM INTO '%1'").arg(escapedLiteral(target));
    if (!query.exec(sql)) {
        if (error)
            *error = query.lastError().text();
        return false;
    }

    if (!QFileInfo::exists(target) || QFileInfo(target).size() <= 0) {
        if (error)
            *error = QStringLiteral("备份文件生成失败");
        return false;
    }

    prune(dbPath);
    if (outPath)
        *outPath = target;
    return true;
}

void BackupService::prune(const QString &dbPath, int keep)
{
    const QStringList files = snapshotFiles(dbPath);
    if (files.size() <= keep)
        return;
    for (int i = 0; i < files.size() - keep; ++i)
        QFile::remove(files.at(i));
}

QDateTime BackupService::lastBackupTime(const QString &dbPath)
{
    const QStringList files = snapshotFiles(dbPath);
    if (files.isEmpty())
        return {};
    return QFileInfo(files.last()).lastModified();
}

int BackupService::backupCount(const QString &dbPath)
{
    return snapshotFiles(dbPath).size();
}

bool BackupService::ensureDailyBackup(QSqlDatabase db, const QString &dbPath, int minHours,
                                      QString *error)
{
    const QDateTime last = lastBackupTime(dbPath);
    if (last.isValid() && last.secsTo(QDateTime::currentDateTime()) < minHours * 3600)
        return true;
    return backupNow(db, dbPath, nullptr, error);
}

bool BackupService::snapshotBeforeUpgrade(const QString &dbPath, QString *error)
{
    if (!QFileInfo::exists(dbPath) || QFileInfo(dbPath).size() <= 0)
        return true;  // 新库，无需备份

    const QString connection = QStringLiteral("keep_backup_probe");
    bool upgradePending = false;
    {
        QSqlDatabase probe = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connection);
        probe.setDatabaseName(dbPath);
        if (!probe.open()) {
            if (error)
                *error = probe.lastError().text();
            probe = QSqlDatabase();
            QSqlDatabase::removeDatabase(connection);
            return false;
        }

        int version = 0;
        {
            QSqlQuery query(probe);
            if (query.exec(QStringLiteral("SELECT MAX(version) FROM schema_version")) && query.next())
                version = query.value(0).toInt();
        }

        int latest = 0;
        for (const Migration &step : Migrator::migrations())
            latest = qMax(latest, step.version);

        if (version < latest) {
            upgradePending = true;
            QString backupError;
            const bool ok = backupNow(probe, dbPath, nullptr, &backupError);
            if (!ok) {
                if (error)
                    *error = backupError;
            }
        }

        probe.close();
    }
    QSqlDatabase::removeDatabase(connection);

    return !upgradePending || (error == nullptr || error->isEmpty());
}

}  // namespace keep
