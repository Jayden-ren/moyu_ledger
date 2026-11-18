#pragma once

#include <QSqlDatabase>
#include <QString>

namespace keep {

// 键值设置（settings 表）：主题、窗口状态等轻量配置
class SettingsRepository
{
public:
    explicit SettingsRepository(QSqlDatabase db);

    [[nodiscard]] QString value(const QString &key, const QString &fallback = QString()) const;
    [[nodiscard]] bool boolValue(const QString &key, bool fallback = false) const;

    [[nodiscard]] bool setValue(const QString &key, const QString &value,
                                QString *error = nullptr);

private:
    QSqlDatabase m_db;
};

}  // namespace keep