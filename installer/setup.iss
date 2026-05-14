#define MyAppName "五子棋 AI"
#define MyAppVersion "2.0"
#define MyAppPublisher "Gomoku AI"
#define MyAppExeName "GomokuAI.exe"

[Setup]
AppId={{3F8B9C2A-1D5E-4A67-B3F8-9C2A1D5E4A67}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\GomokuAI
DefaultGroupName={#MyAppName}
AllowNoIcons=yes
OutputDir=..\installer
OutputBaseFilename=GomokuAI-Setup-2.0
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
UninstallDisplayName={#MyAppName}
UninstallDisplayIcon={app}\{#MyAppExeName}
PrivilegesRequired=lowest
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Languages\ChineseSimplified.isl"

[Messages]
chinesesimp.BeveledLabel=五子棋 AI

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式(&D)"; GroupDescription: "其他:"

[Files]
Source: "..\dist\GomokuAI.exe"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"
Name: "{group}\卸载 {#MyAppName}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "启动 {#MyAppName}"; Flags: nowait postinstall skipifsilent

[Code]
function InitializeSetup: Boolean;
var
  WinVer: TWindowsVersion;
begin
  Result := True;

  // Check Windows version >= 7
  GetWindowsVersionEx(WinVer);
  if (WinVer.Major < 6) or ((WinVer.Major = 6) and (WinVer.Minor < 1)) then
  begin
    MsgBox('需要 Windows 7 或更高版本的系统。' #13#13
           '检测到您的系统版本: Windows ' + IntToStr(WinVer.Major) + '.' + IntToStr(WinVer.Minor),
           mbCriticalError, MB_OK);
    Result := False;
  end;
end;
