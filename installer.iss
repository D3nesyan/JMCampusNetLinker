[Setup]
AppName=JMCampusNetLinker
AppVersion=1.0.0
PrivilegesRequired=admin
DefaultDirName={autopf}\JMCampusNetLinker
DefaultGroupName=JMCampusNetLinker
OutputBaseFilename=JMCampusNetLinker_Setup
SetupIconFile=jimei_auth_icon.ico
Compression=lzma
SolidCompression=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "创建桌面快捷方式"; GroupDescription: "附加快捷方式"

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\JMCampusNetLinker"; Filename: "{app}\JMCampusNetLinker.exe"; IconFilename: "{app}\JMCampusNetLinker.exe"
Name: "{autodesktop}\JMCampusNetLinker"; Filename: "{app}\JMCampusNetLinker.exe"; IconFilename: "{app}\JMCampusNetLinker.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\JMCampusNetLinker.exe"; Description: "运行 JMCampusNetLinker"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Run"" /v JMCampusNetLinker /f"; Flags: runhidden; RunOnceId: "RemoveAutoStart"
