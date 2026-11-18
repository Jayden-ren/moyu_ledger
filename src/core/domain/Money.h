#pragma once

#include <QString>
#include <QtGlobal>

#include <optional>

namespace keep {

// 金额一律以「分」为单位的 int64 存储，禁止用 double 参与运算。
class Money
{
public:
    constexpr Money() = default;
    explicit constexpr Money(qint64 cents) : m_cents(cents) {}

    [[nodiscard]] static constexpr Money fromCents(qint64 cents) { return Money(cents); }
    [[nodiscard]] static std::optional<Money> fromYuan(const QString &text);

    [[nodiscard]] constexpr qint64 cents() const { return m_cents; }
    [[nodiscard]] constexpr bool isZero() const { return m_cents == 0; }
    [[nodiscard]] constexpr bool isNegative() const { return m_cents < 0; }

    // "¥5,907.90" / "-¥2,184.50"；withSymbol=false 时只返回数字部分
    [[nodiscard]] QString format(bool withSymbol = true) const;
    // 固定带符号："+¥158.00" / "-¥23.00"
    [[nodiscard]] QString formatSigned(bool withSymbol = true) const;

    [[nodiscard]] constexpr Money operator+(Money other) const { return Money(m_cents + other.m_cents); }
    [[nodiscard]] constexpr Money operator-(Money other) const { return Money(m_cents - other.m_cents); }
    [[nodiscard]] constexpr Money operator-() const { return Money(-m_cents); }
    constexpr Money &operator+=(Money other) { m_cents += other.m_cents; return *this; }
    constexpr Money &operator-=(Money other) { m_cents -= other.m_cents; return *this; }
    [[nodiscard]] constexpr bool operator==(Money other) const { return m_cents == other.m_cents; }
    [[nodiscard]] constexpr bool operator<(Money other) const { return m_cents < other.m_cents; }

private:
    qint64 m_cents = 0;
};

}  // namespace keep