#pragma once

#include <QChar>
#include <QString>

namespace keep {
namespace Privacy {

// 全局「金额隐藏」开关：打开后 Money::format 会把数字替换成 *
inline bool &flagRef()
{
    static bool value = false;
    return value;
}

inline bool enabled()
{
    return flagRef();
}

inline void setEnabled(bool on)
{
    flagRef() = on;
}

// 只把数字替换成 *，保留货币符号、逗号、小数点与正负号
inline QString mask(const QString &text)
{
    if (!enabled())
        return text;
    QString out = text;
    for (QChar &ch : out) {
        if (ch.isDigit())
            ch = QLatin1Char('*');
    }
    return out;
}

// 导出等需要真实数字的场景：临时关闭掩码
class ScopedMaskOff
{
public:
    ScopedMaskOff() : m_previous(enabled()) { setEnabled(false); }
    ~ScopedMaskOff() { setEnabled(m_previous); }
    ScopedMaskOff(const ScopedMaskOff &) = delete;
    ScopedMaskOff &operator=(const ScopedMaskOff &) = delete;

private:
    bool m_previous;
};

}  // namespace Privacy
}  // namespace keep
