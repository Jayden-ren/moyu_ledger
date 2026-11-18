#include "core/data/CategoryRepository.h"

#include "core/data/Mapping.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

namespace keep {

namespace {

constexpr auto kSelectColumns = "id, parent_id, name, kind, icon, color, sort_order, is_builtin, is_archived";

struct BuiltinCategory
{
    const char *name;
    const char *icon;
    const char *color;
    CategoryKind kind;
};

// 与设计基准第 4.3 节的分类色一致
constexpr BuiltinCategory kBuiltins[] = {
    {"住房", "住", "#6366F1", CategoryKind::Expense},
    {"购物", "购", "#F59E0B", CategoryKind::Expense},
    {"餐饮", "餐", "#EF4444", CategoryKind::Expense},
    {"人情", "人", "#EC4899", CategoryKind::Expense},
    {"教育", "学", "#8B5CF6", CategoryKind::Expense},
    {"娱乐", "娱", "#06B6D4", CategoryKind::Expense},
    {"通讯", "讯", "#64748B", CategoryKind::Expense},
    {"日用", "日", "#22C55E", CategoryKind::Expense},
    {"医疗", "医", "#14B8A6", CategoryKind::Expense},
    {"交通", "交", "#3B82F6", CategoryKind::Expense},
    {"工资", "薪", "#16A34A", CategoryKind::Income},
    {"奖金", "奖", "#22C55E", CategoryKind::Income},
    {"理财", "理", "#3B82F6", CategoryKind::Income},
    {"其他收入", "其", "#64748B", CategoryKind::Income},
};

void setError(QString *error, const QSqlQuery &query)
{
    if (error)
        *error = query.lastError().text();
}

}  // namespace

CategoryRepository::CategoryRepository(QSqlDatabase db)
    : m_db(std::move(db))
{
}

QVector<Category> CategoryRepository::listAll(bool includeArchived, QString *error) const
{
    QVector<Category> categories;
    QSqlQuery query(m_db);
    const QString sql = QStringLiteral("SELECT %1 FROM categories %2 ORDER BY kind, sort_order, id")
                            .arg(QString::fromLatin1(kSelectColumns),
                                 includeArchived ? QString() : QStringLiteral("WHERE is_archived = 0"));

    if (!query.exec(sql)) {
        setError(error, query);
        return categories;
    }

    while (query.next())
        categories.append(detail::categoryFromQuery(query));
    return categories;
}

QVector<Category> CategoryRepository::listByKind(CategoryKind kind, bool includeArchived,
                                                QString *error) const
{
    QVector<Category> categories;
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM categories WHERE kind = ? %2 "
                                 "ORDER BY sort_order, id")
                      .arg(QString::fromLatin1(kSelectColumns),
                           includeArchived ? QString() : QStringLiteral("AND is_archived = 0")));
    query.addBindValue(toDbString(kind));

    if (!query.exec()) {
        setError(error, query);
        return categories;
    }

    while (query.next())
        categories.append(detail::categoryFromQuery(query));
    return categories;
}

std::optional<Category> CategoryRepository::findById(qint64 id, QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM categories WHERE id = ?")
                      .arg(QString::fromLatin1(kSelectColumns)));
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return detail::categoryFromQuery(query);
}

std::optional<Category> CategoryRepository::findByName(const QString &name, CategoryKind kind,
                                                       QString *error) const
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("SELECT %1 FROM categories WHERE name = ? AND kind = ? "
                                 "ORDER BY is_archived LIMIT 1")
                      .arg(QString::fromLatin1(kSelectColumns)));
    query.addBindValue(name);
    query.addBindValue(toDbString(kind));

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    if (!query.next())
        return std::nullopt;
    return detail::categoryFromQuery(query);
}

int CategoryRepository::count(QString *error) const
{
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT COUNT(*) FROM categories")) || !query.next()) {
        setError(error, query);
        return -1;
    }
    return query.value(0).toInt();
}

std::optional<qint64> CategoryRepository::insert(const Category &category, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO categories(parent_id, name, kind, icon, color, sort_order, is_builtin, is_archived) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?)"));
    query.addBindValue(category.parentId ? QVariant(*category.parentId) : QVariant());
    query.addBindValue(category.name);
    query.addBindValue(toDbString(category.kind));
    query.addBindValue(category.icon);
    query.addBindValue(category.color);
    query.addBindValue(category.sortOrder);
    query.addBindValue(category.builtin ? 1 : 0);
    query.addBindValue(category.archived ? 1 : 0);

    if (!query.exec()) {
        setError(error, query);
        return std::nullopt;
    }
    return query.lastInsertId().toLongLong();
}


bool CategoryRepository::update(const Category &category, QString *error)
{
    if (category.name.trimmed().isEmpty()) {
        if (error)
            *error = QStringLiteral("分类名称不能为空");
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "UPDATE categories SET name = ?, icon = ?, color = ?, sort_order = ? WHERE id = ?"));
    query.addBindValue(category.name.trimmed());
    query.addBindValue(category.icon);
    query.addBindValue(category.color);
    query.addBindValue(category.sortOrder);
    query.addBindValue(category.id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.numRowsAffected() > 0;
}

bool CategoryRepository::setArchived(qint64 id, bool archived, QString *error)
{
    QSqlQuery query(m_db);
    query.prepare(QStringLiteral("UPDATE categories SET is_archived = ? WHERE id = ?"));
    query.addBindValue(archived ? 1 : 0);
    query.addBindValue(id);

    if (!query.exec()) {
        setError(error, query);
        return false;
    }
    return query.numRowsAffected() > 0;
}


namespace {
struct BuiltinSub
{
    const char *parent;  // 大类名称
    const char *names[10];
};

constexpr BuiltinSub kSubcategories[] = {
    {"餐饮", {"早餐", "午餐", "晚餐", "外卖", "咖啡奶茶", "聚餐", "买菜", "零食", nullptr}},
    {"交通", {"地铁公交", "打车", "加油", "停车", "高铁机票", "共享单车", nullptr}},
    {"购物", {"日用百货", "数码", "服饰", "家居", "美妆", nullptr}},
    {"住房", {"房租", "房贷", "物业", "水电燃气", "宽带", nullptr}},
    {"人情", {"红包", "请客", "随礼", "借出", "代付", nullptr}},
    {"教育", {"课程", "书籍", "培训", "考试", nullptr}},
    {"娱乐", {"电影演出", "游戏", "会员订阅", "旅行", nullptr}},
    {"通讯", {"话费", "流量", "宽带", nullptr}},
    {"日用", {"洗护", "纸品", "清洁", "宠物", nullptr}},
    {"医疗", {"门诊", "药品", "体检", "牙科", nullptr}},
    {"工资", {"月薪", "年终奖", "补贴", nullptr}},
};
}  // namespace


bool CategoryRepository::seedInvestmentCategories(QString *error)
{
    struct Extra
    {
        const char *name;
        const char *icon;
        const char *color;
        CategoryKind kind;
    };
    constexpr Extra extras[] = {
        {"投资收益", "投", "#10B981", CategoryKind::Income},
        {"投资亏损", "亏", "#EF4444", CategoryKind::Expense},
        {"手续费", "费", "#F59E0B", CategoryKind::Expense},
    };

    for (const Extra &extra : extras) {
        const QString name = QString::fromUtf8(extra.name);
        if (findByName(name, extra.kind).has_value())
            continue;
        Category category;
        category.kind = extra.kind;
        category.name = name;
        category.icon = QString::fromUtf8(extra.icon);
        category.color = QString::fromLatin1(extra.color);
        category.builtin = true;
        category.sortOrder = 200 + count();
        if (!insert(category, error).has_value())
            return false;
    }
    return true;
}

bool CategoryRepository::seedSubcategories(QString *error)
{
    // 幂等：已经有任何小类就不再播种
    {
        QSqlQuery check(m_db);
        if (!check.exec(QStringLiteral(
                "SELECT COUNT(*) FROM categories WHERE parent_id IS NOT NULL"))) {
            setError(error, check);
            return false;
        }
        if (check.next() && check.value(0).toInt() > 0)
            return true;
    }

    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    for (const BuiltinSub &sub : kSubcategories) {
        const QString parentName = QString::fromUtf8(sub.parent);
        const auto parent = findByName(parentName, CategoryKind::Expense);
        const auto parentIncome = findByName(parentName, CategoryKind::Income);
        const auto chosen = parent.has_value() ? parent : parentIncome;
        if (!chosen.has_value())
            continue;

        int order = 0;
        for (int i = 0; i < 10 && sub.names[i] != nullptr; ++i) {
            Category child;
            child.parentId = chosen->id;
            child.name = QString::fromUtf8(sub.names[i]);
            child.kind = chosen->kind;
            child.icon = child.name.left(1);
            child.color = chosen->color;
            child.sortOrder = order++;
            child.builtin = true;
            if (!insert(child, error).has_value()) {
                m_db.rollback();
                return false;
            }
        }
    }

    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }
    return true;
}

bool CategoryRepository::seedBuiltins(QString *error)
{
    const int existing = count(error);
    if (existing < 0)
        return false;
    if (existing > 0)
        return true;

    if (!m_db.transaction()) {
        if (error)
            *error = m_db.lastError().text();
        return false;
    }

    int sortOrder = 0;
    for (const BuiltinCategory &builtin : kBuiltins) {
        Category category;
        category.name = QString::fromUtf8(builtin.name);
        category.icon = QString::fromUtf8(builtin.icon);
        category.color = QString::fromLatin1(builtin.color);
        category.kind = builtin.kind;
        category.builtin = true;
        category.sortOrder = sortOrder++;

        if (!insert(category, error).has_value()) {
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        if (error)
            *error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    return true;
}

}  // namespace keep