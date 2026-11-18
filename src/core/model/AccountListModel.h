#pragma once

#include "core/domain/Domain.h"

#include <QAbstractListModel>
#include <QVector>

namespace keep {

class AccountListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        NameRole,
        TypeTextRole,
        BalanceTextRole,
        IsNegativeRole,
        BadgeRole,
        ColorRole,
        AccountTypeRole,
    };
    Q_ENUM(Roles)

    explicit AccountListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(m_rows.size()); }

    void setAccounts(QVector<AccountWithBalance> rows);

signals:
    void countChanged();

private:
    QVector<AccountWithBalance> m_rows;
};

}  // namespace keep