# 把「摸鱼记账」打包成单个 exe —— 完整教程

> 适用环境：Windows 10/11 + Qt 6.11.1 (MinGW 64-bit) + CMake + Ninja
> 本项目已在 `D:\STUDY\C++_QT\MoyuLedger` 验证通过，产物见 `dist\`。

---

## 0. 先搞清楚两种产物

| 产物 | 文件 | 特点 | 适合 |
|---|---|---|---|
| **便携文件夹** | `dist\MoyuLedger-1.0.0-portable.zip` | 解压即用，约 120 MB，启动快 | 自己电脑长期使用、想改文件 |
| **单文件 exe** | `dist\摸鱼记账-1.0.0-单文件.exe` | **只有一个 exe，约 40 MB** | 发给别人、拷到 U 盘、桌面双击就用 |

本教程重点讲**单文件 exe**，因为它才是「一个 exe 就能跑」的形态。

---

## 1. 前置环境

| 组件 | 本项目使用的版本 | 检查命令 |
|---|---|---|
| Qt | 6.11.1，`mingw_64` 套件 | `C:\Qt\6.11.1\mingw_64\bin\qmake.exe -v` |
| MinGW | 13.10（`C:\Qt\Tools\mingw1310_64`） | `g++ --version` |
| CMake | ≥ 3.21（`C:\Qt\Tools\CMake_64`） | `cmake --version` |
| Ninja | 随 Qt 安装（`C:\Qt\Tools\Ninja`） | `ninja --version` |
| PowerShell | 5.1 或 7.x | `$PSVersionTable.PSVersion` |

> 这些路径写在 `packaging\build-package.ps1` 的变量里。**如果你的 Qt 装到别处，改脚本顶部这三个变量**：
> ```powershell
> $QtPath   = "C:\Qt\6.11.1\mingw_64"
> $mingwBin = "C:\Qt\Tools\mingw1310_64\bin"
> $buildDir = "build\release"
> ```

---

## 2. 一条命令打包（推荐）

在项目根目录执行：

```powershell
powershell -ExecutionPolicy Bypass -File packaging\build-single-exe.ps1
```

看到最后几行就成功了：

```
==> [4/4] 完成
    单文件程序：D:\STUDY\C++_QT\MoyuLedger\dist\摸鱼记账-1.0.0-单文件.exe（40 MB）
    首次运行解压到 %LOCALAPPDATA%\MoyuLedger\app-1.0.0（约 120 MB），
    数据目录 %LOCALAPPDATA%\MoyuLedger\fresh（开箱即空）。
    想使用已有账本数据：摸鱼记账-1.0.0-单文件.exe --use-default-data
```

常用参数：

```powershell
# 只重编译启动器，复用已有便携包（改启动器代码后加速，约 10 秒）
powershell -ExecutionPolicy Bypass -File packaging\build-single-exe.ps1 -SkipPackage

# 指定版本号（会产出 摸鱼记账-1.1.0-单文件.exe）
powershell -ExecutionPolicy Bypass -File packaging\build-single-exe.ps1 -Version 1.1.0
```

---

## 3. 脚本到底做了什么（四步）

```
[1/4] build-package.ps1
        CMake Release 配置 → 编译 → cmake --install（内部调 windeployqt）
        → 精简（删 qmltooling、qt_help 翻译，-Slim 再删软件 OpenGL）
        → Compress-Archive 打成 dist\MoyuLedger-<版本>-portable.zip

[2/4] windres
        把 zip 写成 Windows 资源脚本 app.rc：  1 RCDATA "…\MoyuLedger-1.0.0-portable.zip"
        windres -i app.rc -o app.res -O coff

[3/4] g++ 编译启动器
        g++ -O2 -s -mwindows -municode -DUNICODE -D_UNICODE ^
            -DKEEP_VERSION_HEADER -Ibuild\launcher ^
            packaging\launcher\main.cpp build\launcher\app.res ^
            -o dist\摸鱼记账-1.0.0-单文件.exe -lshell32

[4/4] 完成
```

关键点：**exe 之所以是「单文件」，是因为便携包（zip）被当成 RCDATA 资源塞进了 exe 里**；
启动器启动时把资源解压到本地目录，再拉起真正的 `MoyuLedger.exe`。

---

## 4. 首次运行时发生了什么

```
双击 摸鱼记账-1.0.0-单文件.exe
   │
   ├─ 检查 %LOCALAPPDATA%\MoyuLedger\app-1.0.0\payload.version
   │    · 不存在 / 版本或构建时间不一致 → 清空该目录，从 exe 资源解压（约 5 秒）
   │    · 一致 → 跳过解压，直接启动（约 1 秒）
   │
   ├─ 启动 %LOCALAPPDATA%\MoyuLedger\app-1.0.0\MoyuLedger.exe
   │
   └─ 数据目录：%LOCALAPPDATA%\MoyuLedger\fresh\data.db（开箱即空）
```

- **版本戳**：`payload.version` 里写的是「版本号 + 启动器构建时间」。
  只要重新打包过，旧运行目录会自动作废并重新解压，不会继续跑旧版本。
- **数据在哪**：单文件版默认用 `fresh` 目录，和开发时用的 `%LOCALAPPDATA%\MoyuLedger\data.db` 是**两套**，互不影响。
- **想用已有账本**：

```powershell
.\摸鱼记账-1.0.0-单文件.exe --use-default-data
```

---
## 5. 手动复现（不用脚本，理解每一步）

```powershell
# 0) 准备环境变量
$env:PATH = "C:\Qt\6.11.1\mingw_64\bin;C:\Qt\Tools\mingw1310_64\bin;C:\Qt\Tools\Ninja;C:\Qt\Tools\CMake_64\bin;" + $env:PATH

# 1) Release 构建
cmake -S . -B build\release -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=C:\Qt\6.11.1\mingw_64"
cmake --build build\release

# 2) 安装到 dist\MoyuLedger（会调 windeployqt 把 Qt DLL/插件/QML 拷齐）
cmake --install build\release --prefix dist\MoyuLedger

# 3) 打 zip
Compress-Archive -Path dist\MoyuLedger\* -DestinationPath dist\MoyuLedger-1.0.0-portable.zip -CompressionLevel Optimal

# 4) 把 zip 做成资源
'1 RCDATA "D:\\STUDY\\C++_QT\\MoyuLedger\\dist\\MoyuLedger-1.0.0-portable.zip"' | Set-Content build\launcher\app.rc -Encoding ASCII
windres -i build\launcher\app.rc -o build\launcher\app.res -O coff

# 5) 生成版本头文件
"#pragma once`n#define APP_VERSION_W L`"1.0.0`"" | Set-Content build\launcher\version.h -Encoding UTF8

# 6) 编译启动器（-mwindows 无控制台，-municode 支持 wWinMain）
g++ -O2 -s -mwindows -municode -DUNICODE -D_UNICODE -DKEEP_VERSION_HEADER ^
    -Ibuild\launcher packaging\launcher\main.cpp build\launcher\app.res ^
    -o dist\摸鱼记账-1.0.0-单文件.exe -lshell32
```

> Release 构建需要 `-DCMAKE_PREFIX_PATH` 指向 Qt 套件；用 MinGW Makefiles 生成器也可以，
> 但本项目脚本统一用 Ninja。

---

## 6. 改版本号 / 改应用名

| 要改的东西 | 文件 | 位置 |
|---|---|---|
| 版本号 | `CMakeLists.txt` | `project(MoyuLedger VERSION 1.0.0 LANGUAGES CXX)` |
| 打包版本号 | 命令行 | `-Version 1.1.0` |
| 安装包版本 | `packaging\installer.iss` | `#define MyAppVersion "1.0.0"` |
| 窗口标题/应用名 | `src\ui\Main.qml` / `src\main.cpp` | `appWindow.title` / `setApplicationName` |
| exe 图标 | `packaging\launcher\main.cpp` 的资源脚本 | 当前未内置图标，可加 `IDI_ICON1 ICON "app.ico"` |

---

## 7. 打包后自检清单

```powershell
# 1) 文件存在、大小合理（约 40 MB）
Get-Item dist\摸鱼记账-1.0.0-单文件.exe | Select-Object Name, @{n='MB';e={[math]::Round($_.Length/1MB,1)}}

# 2) 模拟「全新机器」：删掉运行目录再启动
Remove-Item "$env:LOCALAPPDATA\KeepAccount\app-1.0.0" -Recurse -Force -ErrorAction SilentlyContinue
Start-Process dist\摸鱼记账-1.0.0-单文件.exe
# 期望：约 5 秒出现窗口；%LOCALAPPDATA%\MoyuLedger\app-1.0.0\payload.version 有内容

# 3) 再次启动应更快（跳过解压，约 1 秒）

# 4) 数据目录检查（应为空库：0 账户 / 0 流水 / 14 大类 / 51 小类）
python tools\reset_data.py --dry-run --db "$env:LOCALAPPDATA\KeepAccount\fresh\data.db"
```

另外两个快速检查：

```powershell
# 程序自检（147 项断言，不碰用户数据）
build\release\MoyuLedger.exe --self-test

# 数据库结构检查
build\release\MoyuLedger.exe --db-check
```

---

## 8. 常见问题

| 现象 | 原因 | 解决 |
|---|---|---|
| `cmake 配置失败：找不到 Qt6` | `CMAKE_PREFIX_PATH` 没指向 Qt 套件 | 改成 `C:\Qt\6.11.1\mingw_64` |
| 启动报 `找不到 Qt6Core.dll` | 没走 `cmake --install` / `windeployqt` | 重跑 `build-package.ps1`，它会自动部署 |
| 界面能开但控件空白 / 提示 `module "MoyuLedger" is not installed` | 本项目自身 QML 是编译进 exe 资源的（`qt_add_qml_module`），出现该提示通常说明资源没编进去或混用了旧的 build 目录 | 删掉 `build\release` 重新打包；`dist\MoyuLedger\qml\` 里只放 Qt 自带的 QML 模块（QtQuick 等） |
| `windres: can't open file` | zip 路径含中文或反斜杠转义错 | 资源脚本里的路径必须双反斜杠；项目放纯英文路径最稳 |
| 双击没反应 | 单文件版首次要解压约 5 秒；或被杀毒软件拦 | 等几秒；把 exe 加入白名单 |
| 杀毒/Defender 报毒 | 自解压 + 无签名 exe 的常见误报 | 用代码签名证书签名；或直接用便携 zip |
| 每次启动都重新解压 | 启动器重新编译过（版本戳含构建时间） | 正常现象，重打包后第一次必然解压 |
| 想彻底卸载 | 单文件版不写注册表 | 删 `%LOCALAPPDATA%\MoyuLedger` 整个目录即可（数据 + 运行目录） |
| 改了代码但 exe 没变 | 用了 `-SkipPackage` | 去掉该参数重跑，或先删 `build\release` |

---

## 9. 想要「安装版」（带卸载、开始菜单）

项目已准备 `packaging\installer.iss`（Inno Setup 6 脚本）：

1. 安装 [Inno Setup 6](https://jrsoftware.org/isdl.php)
2. 重跑 `packaging\build-package.ps1`，脚本检测到 `ISCC.exe` 会自动产出 `dist\MoyuLedger-1.0.0-setup.exe`
3. 没有 Inno Setup 时会跳过安装包，只产出便携 zip（不影响单文件 exe）

---

## 10. 文件清单（打包相关）

```
packaging/
├── build-package.ps1      # Release 构建 + windeployqt 部署 + 精简 + 打 zip（+可选安装包）
├── build-single-exe.ps1   # 在 zip 基础上生成单文件 exe（推荐用这个）
├── launcher/main.cpp      # 单文件 exe 的启动器：解压资源 → 启动主程序
└── installer.iss          # Inno Setup 安装包脚本

dist/
├── MoyuLedger-1.0.0-portable.zip    # 便携包
├── MoyuLedger/                      # 已部署的便携目录（可直接运行 MoyuLedger.exe）
└── 摸鱼记账-1.0.0-单文件.exe            # ★ 单文件产物
```