#pragma once

#include <QSqlDatabase>
#include <QString>

namespace keep {

// 开发/演示用种子数据：5 个账户 + 设计基准第 4.5 节的 17 笔流水。
// 期初余额经过反推，保证「当前余额」与设计稿完全一致（净资产 ¥30,896.87）。
// 幂等：库里已有账户时直接返回 true。
class DemoSeeder
{
public:
    [[nodiscard]] static bool seed(QSqlDatabase db, QString *error = nullptr);
};

}  // namespace keep