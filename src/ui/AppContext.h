#pragma once

#include <QDate>
#include <QObject>
#include <QQmlEngine>
#include <QString>

// QML 单例 AppContext：全局应用状态（当前月份、账本信息）。
// M1 接入数据层后，月份应改由 LedgerService 驱动；当前只提供时间上下文。
class AppContext : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(QString ledgerName READ ledgerName CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(int year READ year NOTIFY monthChanged)
    Q_PROPERTY(int month READ month NOTIFY monthChanged)
    Q_PROPERTY(QString monthKey READ monthKey NOTIFY monthChanged)
    Q_PROPERTY(QString currentMonth READ currentMonth NOTIFY monthChanged)
    Q_PROPERTY(bool darkTheme READ darkTheme WRITE setDarkTheme NOTIFY darkThemeChanged)

public:
    explicit AppContext(QObject *parent = nullptr);

    QString ledgerName() const;
    QString appVersion() const;
    int year() const;
    int month() const;
    QString monthKey() const;      // "2026-11"
    QString currentMonth() const;  // "2026 年 11 月"

    Q_INVOKABLE void previousMonth();
    Q_INVOKABLE void nextMonth();
    Q_INVOKABLE void goToCurrentMonth();
    Q_INVOKABLE void toggleDarkTheme();

    [[nodiscard]] bool darkTheme() const { return m_darkTheme; }
    void setDarkTheme(bool dark);

signals:
    void monthChanged();
    void darkThemeChanged();

private:
    QDate m_month;  // 当前月份的第一天
    bool m_darkTheme = false;
};
