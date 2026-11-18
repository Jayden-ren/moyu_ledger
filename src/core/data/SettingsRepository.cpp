#include "core/data/SettingsRepository.h"

#include <QSqlError>
#include <QSqlQuery>

namespace keep {

SettingsRepository::SettingsRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QString SettingsRepository::value(const QString &key, const QString &fallback) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key = ?"));
    query.addBindValue(key);
    if (!query.exec() || !query.next())
        return fallback;
    return query.value(0).toString();
}

bool SettingsRepository::boolValue(const QString &key, bool fallback) const
{
    const QString raw = value(key);
    if (raw.isEmpty())
        return fallback;
    return raw == QLatin1String("1") || raw.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0;
}

bool SettingsRepository::setValue(const QString &key, const QString &value, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO settings(key, value) VALUES (?, ?) "
        "ON CONFLICT(key) DO UPDATE SET value = excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    if (!query.exec()) {
        if (error)
            *error = query.lastError().text();
        return false;
    }
    return true;
}

}  // namespace keep