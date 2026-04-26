[Setup]
AppName=校园网认证工具
AppVersion=1.0.0
PrivilegesRequired=admin
DefaultDirName={autopf}\CampusNetTool
DefaultGroupName=校园网认证工具
OutputBaseFilename=CampusNetTool_Setup
Compression=lzma
SolidCompression=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\校园网认证工具"; Filename: "{app}\JMCampusNetLinker.exe"
Name: "{autodesktop}\校园网认证工具"; Filename: "{app}\JMCampusNetLinker.exe"

[Run]
Filename: "{app}\JMCampusNetLinker.exe"; Description: "启动校园网认证工具"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Run"" /v CampusNetTool /f"; Flags: runhidden
