# -*- coding: utf-8 -*-
"""把某个数据目录重置为「全新安装」状态：清空用户数据，保留内置分类与默认账本。

用法：
    python tools\reset_data.py                # 重置默认数据目录（先自动备份）
    python tools\reset_data.py --dry-run      # 只看会删什么，不写库
    python tools\reset_data.py --db <path>    # 重置指定数据库

清理内容：
    流水、标签、账户（含余额）、预算、定期账单、自定义分类、非默认账本、settings
保留内容：
    14 个内置大类 + 51 个内置小类、默认账本「个人账本」
"""
import argparse
import os
import shutil
import sqlite3
import sys
from datetime import datetime

DEFAULT_DB = os.path.join(os.environ.get("LOCALAPPDATA", ""), "MoyuLedger", "data.db")


def count(con, sql):
    return con.execute(sql).fetchone()[0]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--db", default=DEFAULT_DB)
    parser.add_argument("--dry-run", action="store_true")
    args = parser.parse_args()

    path = args.db
    if not os.path.exists(path):
        print("数据库不存在：%s" % path)
        return 1

    con = sqlite3.connect(path)
    con.execute("PRAGMA foreign_keys = ON")

    print("数据库：%s" % path)
    print("   账户 %d / 流水 %d / 预算 %d / 定期账单 %d / 标签 %d / 账本 %d / 非内置分类 %d / 设置 %d"
          % (count(con, "select count(*) from accounts"),
             count(con, "select count(*) from transactions"),
             count(con, "select count(*) from budgets"),
             count(con, "select count(*) from recurring_rules"),
             count(con, "select count(*) from tags"),
             count(con, "select count(*) from ledgers"),
             count(con, "select count(*) from categories where is_builtin = 0"),
             count(con, "select count(*) from settings")))

    if args.dry_run:
        print("（dry-run，不做修改）")
        con.close()
        return 0

    stamp = datetime.now().strftime("%Y%m%d-%H%M%S")
    backup = "%s.bak-%s" % (path, stamp)
    shutil.copy2(path, backup)
    print("已备份 -> %s" % backup)

    try:
        con.execute("BEGIN")
        # 依赖顺序：标签关联 → 标签 → 流水 → 预算 → 定期账单 → 账户
        con.execute("DELETE FROM transaction_tags")
        con.execute("DELETE FROM tags")
        con.execute("DELETE FROM transactions")
        con.execute("DELETE FROM budgets")
        con.execute("DELETE FROM recurring_rules")
        con.execute("DELETE FROM accounts")
        # 去掉自定义分类 / 非默认账本 / 设置
        con.execute("DELETE FROM categories WHERE is_builtin = 0")
        con.execute("DELETE FROM ledgers WHERE is_default = 0")
        con.execute("DELETE FROM settings")
        # 让新数据的自增 id 从 1 开始
        for table in ("transactions", "accounts", "budgets", "tags", "recurring_rules"):
            con.execute("DELETE FROM sqlite_sequence WHERE name = ?", (table,))
        con.commit()
    except Exception as exc:  # noqa: BLE001
        con.rollback()
        print("清理失败，已回滚：%s" % exc)
        con.close()
        return 2

    con.execute("VACUUM")
    print("清理后：账户 %d / 流水 %d / 预算 %d / 定期账单 %d / 标签 %d / 账本 %d / 分类(大类/小类) %d/%d / 设置 %d"
          % (count(con, "select count(*) from accounts"),
             count(con, "select count(*) from transactions"),
             count(con, "select count(*) from budgets"),
             count(con, "select count(*) from recurring_rules"),
             count(con, "select count(*) from tags"),
             count(con, "select count(*) from ledgers"),
             count(con, "select count(*) from categories where parent_id is null"),
             count(con, "select count(*) from categories where parent_id is not null"),
             count(con, "select count(*) from settings")))
    con.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())