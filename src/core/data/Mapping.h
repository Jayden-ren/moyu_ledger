#pragma once

#include "core/domain/Domain.h"

#include <QDateTime>
#include <QSqlQuery>
#include <QVariant>

namespace keep::detail {

inline QDateTime parseTimestamp(const QString &text)
{
    QDateTime value = QDateTime::fromString(text, Qt::ISODate);
    if (!value.isValid())
        value = QDateTime::fromString(text, QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
    return value;
}

// 纯本地账本：一律用「本地时间、无时区后缀」存储，避免读写往返出现 8 小时偏移
inline QString timestampText(const QDateTime &value)
{
    return value.toString(QStringLiteral("yyyy-MM-ddTHH:mm:ss"));
}

inline Account accountFromQuery(const QSqlQuery &query)
{
    Account account;
    account.id = query.value(QStringLiteral("id")).toLongLong();
    account.name = query.value(QStringLiteral("name")).toString();

    const auto type = accountTypeFromDb(query.value(QStringLiteral("type")).toString());
    account.type = type.value_or(AccountType::Savings);

    account.currency = query.value(QStringLiteral("currency")).toString();
    account.openingBalance = Money::fromCents(query.value(QStringLiteral("opening_balance_cents")).toLongLong());

    const QVariant limit = query.value(QStringLiteral("credit_limit_cents"));
    if (!limit.isNull())
        account.creditLimitCents = limit.toLongLong();

    account.icon = query.value(QStringLiteral("icon")).toString();
    account.color = query.value(QStringLiteral("color")).toString();
    account.sortOrder = query.value(QStringLiteral("sort_order")).toInt();
    account.archived = query.value(QStringLiteral("is_archived")).toInt() != 0;
    const QVariant ledgerValue = query.value(QStringLiteral("ledger_id"));
    if (!ledgerValue.isNull())
        account.ledgerId = ledgerValue.toLongLong();
    return account;
}

inline Category categoryFromQuery(const QSqlQuery &query)
{
    Category category;
    category.id = query.value(QStringLiteral("id")).toLongLong();

    const QVariant parent = query.value(QStringLiteral("parent_id"));
    if (!parent.isNull())
        category.parentId = parent.toLongLong();

    category.name = query.value(QStringLiteral("name")).toString();
    category.kind = categoryKindFromDb(query.value(QStringLiteral("kind")).toString())
                        .value_or(CategoryKind::Expense);
    category.icon = query.value(QStringLiteral("icon")).toString();
    category.color = query.value(QStringLiteral("color")).toString();
    category.sortOrder = query.value(QStringLiteral("sort_order")).toInt();
    category.builtin = query.value(QStringLiteral("is_builtin")).toInt() != 0;
    category.archived = query.value(QStringLiteral("is_archived")).toInt() != 0;
    return category;
}

inline Transaction transactionFromQuery(const QSqlQuery &query)
{
    Transaction transaction;
    transaction.id = query.value(QStringLiteral("id")).toLongLong();
    transaction.kind = transactionKindFromDb(query.value(QStringLiteral("kind")).toString())
                           .value_or(TransactionKind::Expense);
    transaction.amount = Money::fromCents(query.value(QStringLiteral("amount_cents")).toLongLong());
    transaction.accountId = query.value(QStringLiteral("account_id")).toLongLong();

    const QVariant toAccount = query.value(QStringLiteral("to_account_id"));
    if (!toAccount.isNull())
        transaction.toAccountId = toAccount.toLongLong();

    const QVariant category = query.value(QStringLiteral("category_id"));
    if (!category.isNull())
        transaction.categoryId = category.toLongLong();

    transaction.occurredAt = parseTimestamp(query.value(QStringLiteral("occurred_at")).toString());
    transaction.note = query.value(QStringLiteral("note")).toString();
    transaction.payee = query.value(QStringLiteral("payee")).toString();
    transaction.externalId = query.value(QStringLiteral("external_id")).toString();
    return transaction;
}

}  // namespace keep::detail