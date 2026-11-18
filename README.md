# 摸鱼记账（moyu_ledger）

> 上班顺手记一笔的本地记账软件。Qt 6 + C++20 + QML，数据只存本地，不联网、不注册、不打扰。

- **中文名**：摸鱼记账
- **English**：moyu_ledger
- **版本**：1.0.0
- **平台**：Windows 10/11（x64）
- **技术栈**：Qt 6.11.1（QML）、C++20、SQLite（WAL）

---

## 功能

| 模块 | 能力 |
|---|---|
| 记账 | 记一笔（支出 / 收入 / 转账三段；大类 + 小类，可只记大类，也可现场新建；数字键盘录金额；点击弹时分与日历选择；标签快捷 chip；大类预算卡；备注/标签）、编辑、删除、撤销重做 |
| 流水 | 按日分组，列为 分类 · 时间（年月日时分）· 摘要 · 账户 · 金额；单击看右侧交易详情，双击直接编辑，详情面板也有「编辑这笔」 |
| 转账 | 账户互转（不计收支 / 预算）；转给他人默认记作支出，可切「借出」记入应收 |
| 投资 | 理财 / 股票账户每日收益：填收益或填市值（自动反推收益）、佣金单独记支出、历史记录可逐条修复或删除 |
| 账户 | 新增/编辑/归档（含「应收 / 应付」往来账户、「理财 / 股票」投资账户），列对齐列表，净资产实时计算 |
| 预算 | 月度总预算 + 分类预算、进度条与超支提醒；记账面板右下角显示当前大类本月支出/预算 |
| 报表 | 月/季/年切换、分类占比、趋势、账户维度、月度明细、环比同比、标签排行、导出 CSV / PDF / PNG |
| 隐私 | 工具栏眼睛开关（默认关闭）：打开后所有金额数字显示为 `*`，导出仍写真实金额 |
| 备份 | 工具栏「导出 / 导入」：全量 JSON 备份，涵盖账本、账户、分类、全部流水、标签、预算、定期账单、设置；导入前自动备份当前库 |
| 多账本 | 账本切换、重命名、删除；账户与流水按账本隔离 |
| 其他 | 命令面板（Ctrl+K）、全局搜索与筛选、定期账单、分类管理、自动备份（每日）、暗色主题 |

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `Ctrl+N` | 记一笔 |
| `Enter` | 查看选中流水的详情 |
| `Ctrl+E` | 编辑当前流水 |
| `Ctrl+Z` / `Ctrl+Shift+Z` | 撤销 / 重做 |
| `Ctrl+K` | 命令面板 |
| `Ctrl+F` | 聚焦搜索框 |
| `Ctrl+1` … `Ctrl+7` | 切换页面（工作台 / 概览 / 流水 / 报表 / 预算 / 账户 / 设置） |
| `Ctrl+Shift+L` | 切换暗色主题 |
| `Ctrl+,` | 设置 |
| `Esc` | 关闭弹层 / 取消编辑 |

## 构建

**环境**：Qt 6.11.1 (MinGW 64-bit)、MinGW 13.10、CMake ≥ 3.21、Ninja。

```powershell
# 准备环境变量（按你的安装路径调整）
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;" + $env:PATH

# 配置 + 构建 + 运行
cmake -S . -B build\debug -G Ninja -DCMAKE_BUILD_TYPE=Debug "-DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64"
cmake --build build\debug
.\build\debug\MoyuLedger.exe

# 测试
cd build\debug; ctest --output-on-failure    # 1 个测试目标
.\MoyuLedger.exe --self-test                  # 154 项数据/业务断言，用独立临时库
```

> ⚠️ 改动 QML 目录结构或 `CMakeLists.txt` 里的 QML 模块后，**必须删掉构建目录重新配置**，否则 Qt 缓存里的旧 qmldir/资源路径会导致「Type xxx unavailable」。

## 测试

```powershell
# 数据/业务自检：154 项断言，独立临时库，不污染用户数据
.\build\debug\MoyuLedger.exe --self-test

# 库结构检查（表、索引、版本、外键）
.\build\debug\MoyuLedger.exe --db-check

# 演示数据（仅供开发，写入当前库）
.\build\debug\MoyuLedger.exe --seed-demo
```

## 目录结构

```
moyu_ledger/
├── CMakeLists.txt              # 根工程：keep_core 静态库 + 子目录
├── main.cpp                    # 入口：--self-test / --db-check / --seed-demo / 正常启动
├── src/
│   ├── core/
│   │   ├── domain/             # Money / Domain 枚举 / Privacy 掩码
│   │   ├── data/               # SQLite 仓储：账户、流水、分类、预算、标签、账本、备份…
│   │   ├── model/              # LedgerViewModel（唯一对 QML 暴露的业务门面）+ 列表模型
│   │   └── ...
│   └── ui/
│       ├── Main.qml            # 应用外壳：工具条 / 导航 / 页面栈 / 弹层
│       ├── Theme.qml / AppContext.qml
│       ├── pages/              # 工作台、概览、流水、报表、预算、账户、设置
│       ├── components/         # 表格、图表、详情面板、日期/时间选择器等
│       └── dialogs/            # 记一笔、分类、导入、定期账单、每日收益…
├── tests/                      # CTest 包装
├── packaging/                  # 打包脚本 + 单文件启动器 + Inno Setup 脚本
├── tools/                      # 开发工具（QML 引用检查、数据重置）
├── docs/                       # 打包教程
└── 方案设计/                    # UI 原型与设计文档（历史资料）
```

## 单文件版（开箱即空）

产物：`dist\摸鱼记账-1.0.0-单文件.exe`（约 40 MB，无需安装、不依赖 Qt 环境）

- 首次双击运行：自动解压到 `%LOCALAPPDATA%\MoyuLedger\app-1.0.0`（约 120 MB），耗时约 5 秒
- 数据目录独立：`%LOCALAPPDATA%\MoyuLedger\fresh\data.db` —— 0 账户 / 0 流水 / 14 个内置大类 + 51 个小类，不会读取已有账本
- 想用已有账本数据：`摸鱼记账-1.0.0-单文件.exe --use-default-data`
- 卸载：删除该 exe 与 `%LOCALAPPDATA%\MoyuLedger` 目录

> 从旧名「小账本 / KeepAccount」升级时，启动器与程序会自动把 `%LOCALAPPDATA%\KeepAccount` 迁移为 `%LOCALAPPDATA%\MoyuLedger`，数据不丢。

## 打包发布

```powershell
# 便携包 +（装了 Inno Setup 时）安装包
powershell -ExecutionPolicy Bypass -File packaging\build-package.ps1

# 极限精简（删除软件 OpenGL 回退，约省 20MB；仅确定目标机器都有显卡驱动时用）
powershell -ExecutionPolicy Bypass -File packaging\build-package.ps1 -Slim
```

产物都在 `dist/`：

| 文件 | 说明 |
|---|---|
| `dist/MoyuLedger/` | 可直接运行的绿色目录（约 120 MB） |
| `dist/MoyuLedger-1.0.0-portable.zip` | 便携包（约 39 MB） |
| `dist/MoyuLedger-1.0.0-setup.exe` | 安装包（需安装 [Inno Setup 6](https://jrsoftware.org/isinfo.php)，脚本自动调用 `ISCC.exe`） |
| `dist/摸鱼记账-1.0.0-单文件.exe` | 单文件版（内嵌运行库，开箱即空） |

**部署要点**（踩过的坑，改 CMake 时别删）：

- QML 应用必须用 `windeployqt --qmldir src/ui`，否则 QtQuick/Controls 等模块不会进安装目录，运行时报「模块没有安装」。
- 不要给 `windeployqt` 同时传 `--libdir/--plugindir/--qml-deploy-dir`：实测会按当前工作目录解析，把文件写到项目根。安装目录用**平铺布局**（exe 与 Qt 运行库同层）。
- MinGW 构建不要加 `--no-compiler-runtime`，否则缺 `libstdc++-6.dll` 等，启动报 0xC0000135。

## 单文件 exe 打包教程

- **产物**：`dist\摸鱼记账-1.0.0-单文件.exe`（约 40 MB，双击即用）
- **完整教程**：[docs/打包成单个exe教程.md](docs/打包成单个exe教程.md)（环境准备、一键打包、脚本原理、手动复现、常见问题）
- **一条命令**：`powershell -ExecutionPolicy Bypass -File packaging\build-single-exe.ps1`

## 开发工具

```powershell
# 检查 QML 里「引用了不存在的控件 id」（控件被替换后最容易漏改的地方）
python tools\check_qml_refs.py

# 把数据重置为「全新安装」：清空账户/流水/预算/自定义分类/非默认账本/设置
# 保留 14 大类 + 51 小类 + 默认账本；执行前自动备份 data.db.bak-<时间戳>
python tools\reset_data.py --dry-run   # 先看会删什么
python tools\reset_data.py             # 确认执行
```

## 命令行参数

| 参数 | 作用 |
|---|---|
| `--self-test` | 数据/业务自检（**154 项断言**，独立临时库，不污染用户数据） |
| `--db-check` | 打印库结构、表数量、索引、外键与 schema 版本 |
| `--seed-demo` | 写入演示数据（仅开发用） |
| `--use-default-data` | 单文件版专用：使用默认数据目录（读取已有账本） |

## 数据位置

数据文件：`%LOCALAPPDATA%\MoyuLedger\data.db`（删掉该目录即可重置）。

- 自动备份目录：`%LOCALAPPDATA%\MoyuLedger\backups`（每日自动 + 手动备份，保留最近 7 份）
- 单文件版数据目录：`%LOCALAPPDATA%\MoyuLedger\fresh\data.db`
- 可用环境变量覆盖：`MOYU_LEDGER_DB`（指定 db 文件）、`MOYU_LEDGER_HOME`（指定数据目录）

## 设计文档

`方案设计/` 下有完整的历史设计资料：市场调研、7 套 UI 方案（HTML 原型）、设计基准、开发计划（Step 1–36）。

> 说明：原型 PNG 截图因仓库体积限制未入库（本地保留），仓库里只有 HTML 与说明文档。