; 摸鱼记账 安装脚本（Inno Setup 6）
; 用法：ISCC.exe /DMyVersion=1.0.0 packaging\installer.iss
; 安装包输出到 ..\dist\MoyuLedger-<版本>-setup.exe

#ifndef MyVersion
  #define MyVersion "1.0.0"
#endif

#define AppName "摸鱼记账"
#define AppExe "MoyuLedger.exe"
#define AppPublisher "Keep Account"

[Setup]
AppId={{7C4F1E20-9A3B-4F5D-8E21-5B0D7C6A9F31}
AppName={#AppName}
AppVersion={#MyVersion}
AppVerName={#AppName} {#MyVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
OutputDir=..\dist
OutputBaseFilename=MoyuLedger-{#MyVersion}-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
UninstallDisplayIcon={app}\{#AppExe}
MinVersion=10.0

; 代码签名：由 build-package.ps1 传 /DSignCommand=1 与 /Skeep_sign=<签名命令> 时启用
#ifdef SignCommand
SignTool=keep_sign
SignedUninstaller=yes
#endif

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加任务:"; Languages: chinesesimp
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional tasks:"; Languages: english

[Files]
Source: "..\dist\MoyuLedger\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExe}"
Name: "{group}\卸载 {#AppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExe}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExe}"; Description: "启动 {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
Type: filesandordirs; Name: "{app}"