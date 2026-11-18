#pragma once

#include "core/domain/Domain.h"

#include <QAbstractListModel>
#include <QHash>
#include <QString>
#include <QVector>

namespace keep {

// 流水列表模型：一次装载一个月的流水，行内已完成金额/日期/分类的格式化，QML 不再二次计算。
class TransactionListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        KindRole,
        KindTextRole,
        AmountTextRole,
        AmountCentsRole,
        IsExpenseRole,
        CategoryIdRole,
        CategoryNameRole,
        CategoryBadgeRole,
        CategoryColorRole,
        AccountIdRole,
        AccountNameRole,
        OccurredTextRole,
        OccurredFullTextRole,
        DateTextRole,
        DateKeyRole,
        NoteRole,
        PayeeRole,
        TagsTextRole,
        ToAccountIdRole,
        ToAccountNameRole,
    };
    Q_ENUM(Roles)

    explicit TransactionListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(m_rows.size()); }

    void setTransactions(QVector<Transaction> transactions,
                         const QHash<qint64, QString> &accountNames,
                         const QHash<qint64, QString> &categoryNames,
                         const QHash<qint64, QPair<QString, QString>> &categoryBadges,
                         const QHash<qint64, QString> &tagTexts = {});

    // 日期分组头用（ListView section delegate 通过 section 值回查）
    Q_INVOKABLE QString dayLabel(const QString &dateKey) const;
    Q_INVOKABLE QString dayTotalText(const QString &dateKey) const;

    // 详情面板用：整行打包成 map（含格式化后的展示字段）
    Q_INVOKABLE QVariantMap rowData(int row) const;

signals:
    void countChanged();

private:
    struct Row
    {
        Transaction transaction;
        QString accountName;
        QString categoryName;
        QString categoryBadge;
        QString categoryColor;
        QString dateKey;
        QString dateLabel;
        QString tagsText;
        QString toAccountName;
    };

    QString occurredText(const QDateTime &value) const;

    QVector<Row> m_rows;
    QHash<QString, qint64> m_dayIncome;   // dateKey -> 当日收入合计（分）
    QHash<QString, qint64> m_dayExpense;  // dateKey -> 当日支出合计（分）
    QHash<QString, QString> m_dayLabels;
};

}  // namespace keep