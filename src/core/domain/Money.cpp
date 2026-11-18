#include "core/domain/Money.h"

#include "core/domain/Privacy.h"

#include <QChar>
#include <QRegularExpression>
#include <QStringList>

namespace keep {

namespace {

QString groupThousands(qint64 value)
{
    const QString digits = QString::number(value);
    QString grouped;
    grouped.reserve(digits.size() + digits.size() / 3);
    for (int i = 0; i < digits.size(); ++i) {
        if (i > 0 && (digits.size() - i) % 3 == 0)
            grouped += QLatin1Char(',');
        grouped += digits.at(i);
    }
    return grouped;
}

QString symbol()
{
    static const QString yen = QString(QChar(0x00A5));  // ¥
    return yen;
}

}  // namespace

std::optional<Money> Money::fromYuan(const QString &input)
{
    QString text = input.trimmed();
    text.remove(QChar(0x00A5));
    text.remove(QLatin1Char(','));
    text.remove(QLatin1Char(' '));
    text.remove(QChar(0xFFE5));  // 全角 ￥

    bool negative = false;
    if (text.startsWith(QLatin1Char('-'))) {
        negative = true;
        text.remove(0, 1);
    } else if (text.startsWith(QLatin1Char('+'))) {
        text.remove(0, 1);
    }

    static const QRegularExpression pattern(QStringLiteral("^\\d{1,12}(\\.\\d{0,2})?$"));
    if (!pattern.match(text).hasMatch())
        return std::nullopt;

    const int dot = text.indexOf(QLatin1Char('.'));
    bool ok = false;

    qint64 yuanPart = 0;
    qint64 fenPart = 0;

    if (dot < 0) {
        yuanPart = text.toLongLong(&ok);
        if (!ok)
            return std::nullopt;
    } else {
        yuanPart = text.left(dot).toLongLong(&ok);
        if (!ok)
            return std::nullopt;

        const QString fraction = text.mid(dot + 1).leftJustified(2, QLatin1Char('0'));
        fenPart = fraction.toLongLong(&ok);
        if (!ok)
            return std::nullopt;
    }

    const qint64 cents = yuanPart * 100 + fenPart;
    return Money(negative ? -cents : cents);
}

QString Money::format(bool withSymbol) const
{
    const bool negative = m_cents < 0;
    const qint64 absolute = negative ? -m_cents : m_cents;
    const QString number = groupThousands(absolute / 100) + QLatin1Char('.')
                           + QString::number(absolute % 100).rightJustified(2, QLatin1Char('0'));

    if (!withSymbol)
        return Privacy::mask(negative ? QStringLiteral("-") + number : number);

    return Privacy::mask(negative ? QStringLiteral("-%1%2").arg(symbol(), number)
                                  : QStringLiteral("%1%2").arg(symbol(), number));
}

QString Money::formatSigned(bool withSymbol) const
{
    if (m_cents > 0)
        return QStringLiteral("+") + format(withSymbol);
    return format(withSymbol);
}

}  // namespace keep