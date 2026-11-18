#include "core/model/AccountListModel.h"

namespace keep {

AccountListModel::AccountListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AccountListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_rows.size());
}

QVariant AccountListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};

    const AccountWithBalance &row = m_rows.at(index.row());

    switch (role) {
    case IdRole:
        return row.account.id;
    case NameRole:
        return row.account.name;
    case TypeTextRole:
        return displayName(row.account.type);
    case BalanceTextRole:
        return row.balance.format();
    case IsNegativeRole:
        return row.balance.isNegative();
    case BadgeRole:
        return row.account.icon.isEmpty() ? row.account.name.left(1) : row.account.icon;
    case ColorRole:
        return row.account.color;
    case AccountTypeRole:
        return toDbString(row.account.type);
    default:
        return {};
    }
}

QHash<int, QByteArray> AccountListModel::roleNames() const
{
    return {
        {IdRole, "accountId"},
        {NameRole, "name"},
        {TypeTextRole, "typeText"},
        {BalanceTextRole, "balanceText"},
        {IsNegativeRole, "isNegative"},
        {BadgeRole, "badge"},
        {ColorRole, "accentColor"},
        {AccountTypeRole, "accountType"},
    };
}

void AccountListModel::setAccounts(QVector<AccountWithBalance> rows)
{
    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
    emit countChanged();
}

}  // namespace keep