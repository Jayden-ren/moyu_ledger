#include "AppContext.h"

#include <QCoreApplication>

namespace {
QString formatMonth(const QDate &month)
{
    return QStringLiteral("%1 年 %2 月").arg(month.year()).arg(month.month());
}
}  // namespace

AppContext::AppContext(QObject *parent)
    : QObject(parent)
{
    const QDate today = QDate::currentDate();
    m_month = QDate(today.year(), today.month(), 1);
}

QString AppContext::ledgerName() const
{
    return QStringLiteral("个人账本");
}

QString AppContext::appVersion() const
{
    const QString version = QCoreApplication::applicationVersion();
    return version.isEmpty() ? QStringLiteral("1.0.0") : version;
}

int AppContext::year() const
{
    return m_month.year();
}

int AppContext::month() const
{
    return m_month.month();
}

QString AppContext::monthKey() const
{
    return m_month.toString(QStringLiteral("yyyy-MM"));
}

QString AppContext::currentMonth() const
{
    return formatMonth(m_month);
}

void AppContext::previousMonth()
{
    m_month = m_month.addMonths(-1);
    emit monthChanged();
}

void AppContext::nextMonth()
{
    m_month = m_month.addMonths(1);
    emit monthChanged();
}

void AppContext::setDarkTheme(bool dark)
{
    if (m_darkTheme == dark)
        return;
    m_darkTheme = dark;
    emit darkThemeChanged();
}

void AppContext::toggleDarkTheme()
{
    setDarkTheme(!m_darkTheme);
}

void AppContext::goToCurrentMonth()
{
    const QDate today = QDate::currentDate();
    const QDate firstOfMonth(today.year(), today.month(), 1);
    if (firstOfMonth == m_month)
        return;

    m_month = firstOfMonth;
    emit monthChanged();
}
