// 摸鱼记账 单文件启动器
// 内嵌便携包（RCDATA），首次运行解压到 %LOCALAPPDATA%\MoyuLedger\app-<版本>，
// 然后用独立数据目录（MOYU_LEDGER_HOME）启动主程序 —— 保证「开箱即空的干净账本」。
//
// 参数：
//   --use-default-data   不覆盖数据目录，使用程序默认的 %LOCALAPPDATA%\MoyuLedger\data.db
//   其他参数原样转发给主程序

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <filesystem>
#include <fstream>
#include <string>

#ifdef KEEP_VERSION_HEADER
#include "version.h"
#endif

#ifndef APP_VERSION_W
#define APP_VERSION_W L"1.0.0"
#endif

namespace {

constexpr int kResourceId = 1;  // 与 app.rc 中的编号一致

std::wstring envOrEmpty(const wchar_t *name)
{
    DWORD size = GetEnvironmentVariableW(name, nullptr, 0);
    if (size == 0)
        return {};
    std::wstring value(size, L'\0');
    GetEnvironmentVariableW(name, value.data(), size);
    while (!value.empty() && value.back() == L'\0')
        value.pop_back();
    return value;
}

std::wstring joinPath(const std::wstring &a, const std::wstring &b)
{
    if (a.empty())
        return b;
    std::wstring result = a;
    if (result.back() != L'\\' && result.back() != L'/')
        result.push_back(L'\\');
    result += b;
    return result;
}

bool ensureDirectory(const std::wstring &path)
{
    if (path.empty())
        return false;
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))
        return true;

    const std::size_t slash = path.find_last_of(L"\\/");
    if (slash != std::wstring::npos && slash > 3)
        ensureDirectory(path.substr(0, slash));

    return CreateDirectoryW(path.c_str(), nullptr) != 0
           || GetLastError() == ERROR_ALREADY_EXISTS;
}

bool fileExists(const std::wstring &path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && !(attributes & FILE_ATTRIBUTE_DIRECTORY);
}

bool writeResourceToFile(const std::wstring &path)
{
    HMODULE module = GetModuleHandleW(nullptr);
    HRSRC resource = FindResourceW(module, MAKEINTRESOURCEW(kResourceId), RT_RCDATA);
    if (resource == nullptr)
        return false;

    HGLOBAL loaded = LoadResource(module, resource);
    if (loaded == nullptr)
        return false;

    const DWORD size = SizeofResource(module, resource);
    const void *data = LockResource(loaded);
    if (data == nullptr || size == 0)
        return false;

    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
        return false;

    DWORD written = 0;
    const bool ok = WriteFile(file, data, size, &written, nullptr) != 0 && written == size;
    CloseHandle(file);
    return ok;
}

int runAndWait(const std::wstring &commandLine)
{
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};

    std::wstring mutableCommand = commandLine;
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) {
        return -1;
    }

    WaitForSingleObject(process.hProcess, INFINITE);
    DWORD exitCode = 1;
    GetExitCodeProcess(process.hProcess, &exitCode);
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return static_cast<int>(exitCode);
}

std::wstring quote(const std::wstring &value)
{
    std::wstring result = L"\"";
    result += value;
    result.push_back(L'"');
    return result;
}

}  // namespace

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
    // 解析参数：--use-default-data 由启动器消费，其余转发
    int argc = 0;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    bool useDefaultData = false;
    std::wstring forwardedArgs;
    for (int i = 1; i < argc; ++i) {
        if (wcscmp(argv[i], L"--use-default-data") == 0) {
            useDefaultData = true;
            continue;
        }
        forwardedArgs += L" ";
        forwardedArgs += quote(argv[i]);
    }
    if (argv != nullptr)
        LocalFree(argv);

    const std::wstring localAppData = envOrEmpty(L"LOCALAPPDATA");
    if (localAppData.empty()) {
        MessageBoxW(nullptr, L"无法获取 LOCALAPPDATA，启动失败。", L"摸鱼记账", MB_ICONERROR);
        return 1;
    }

    // 改名迁移：旧版本把运行目录与数据放在 KeepAccount，首次启动时整体搬过来
    {
        const std::wstring legacyRoot = joinPath(localAppData, L"KeepAccount");
        const std::wstring appRootProbe = joinPath(localAppData, L"MoyuLedger");
        std::error_code ec;
        if (!std::filesystem::exists(std::filesystem::path(appRootProbe))
            && std::filesystem::exists(std::filesystem::path(legacyRoot))) {
            std::filesystem::rename(std::filesystem::path(legacyRoot),
                                    std::filesystem::path(appRootProbe), ec);
        }
    }

    const std::wstring appRoot = joinPath(localAppData, L"MoyuLedger");
    const std::wstring installDir = joinPath(appRoot, std::wstring(L"app-") + APP_VERSION_W);
    const std::wstring mainExe = joinPath(installDir, L"MoyuLedger.exe");

    // 版本戳：application version 变了就重新解压（否则旧运行目录会一直沿用）
    const std::wstring stampPath = joinPath(installDir, L"payload.version");
    std::string stampValue;
    {
        std::ifstream stampFile{std::filesystem::path(stampPath)};
        if (stampFile)
            std::getline(stampFile, stampValue);
    }
    // 版本戳里带上启动器的构建时间：只要重新打包过，就一定会重新解压
    std::string expectedVersion;
    {
        const std::wstring wide(APP_VERSION_W);
        expectedVersion.assign(wide.begin(), wide.end());
    }
    expectedVersion += " ";
    expectedVersion += __DATE__;
    expectedVersion += " ";
    expectedVersion += __TIME__;
    const bool needExtract = !fileExists(mainExe) || stampValue != expectedVersion;

    if (needExtract) {
        if (fileExists(mainExe) || !stampValue.empty()) {
            std::error_code ec;
            std::filesystem::remove_all(std::filesystem::path(installDir), ec);
        }
        if (!ensureDirectory(installDir)) {
            MessageBoxW(nullptr, L"无法创建程序目录，启动失败。", L"摸鱼记账", MB_ICONERROR);
            return 2;
        }

        wchar_t tempPath[MAX_PATH]{};
        GetTempPathW(MAX_PATH, tempPath);
        const std::wstring payload = joinPath(tempPath,
                                                std::wstring(L"keep_payload_")
                                                    + std::to_wstring(GetCurrentProcessId())
                                                    + L".zip");
        if (!writeResourceToFile(payload)) {
            MessageBoxW(nullptr, L"内置程序包释放失败。", L"摸鱼记账", MB_ICONERROR);
            return 3;
        }

        // 用系统自带的 bsdtar 解压（Windows 10 1803+ 内置）
        const std::wstring tarCommand = std::wstring(L"tar.exe -xf ") + quote(payload) + L" -C "
                                        + quote(installDir);
        const int tarResult = runAndWait(tarCommand);
        DeleteFileW(payload.c_str());
        if (tarResult != 0 || !fileExists(mainExe)) {
            MessageBoxW(nullptr, L"解压程序包失败，请确认系统版本为 Windows 10 及以上。",
                        L"摸鱼记账", MB_ICONERROR);
            return 4;
        }

        std::ofstream stampOut(std::filesystem::path(stampPath), std::ios::trunc);
        stampOut << expectedVersion;
    }

    // 干净账本：默认使用独立数据目录，避免读到用户已有账本
    if (!useDefaultData) {
        const std::wstring dataHome = joinPath(appRoot, L"fresh");
        ensureDirectory(dataHome);
        SetEnvironmentVariableW(L"MOYU_LEDGER_HOME", dataHome.c_str());
    }

    const std::wstring commandLine = quote(mainExe) + forwardedArgs;
    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    std::wstring mutableCommand = commandLine;
    if (!CreateProcessW(nullptr, mutableCommand.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                        &startup, &process)) {
        MessageBoxW(nullptr, L"启动主程序失败。", L"摸鱼记账", MB_ICONERROR);
        return 5;
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return 0;
}