[Setup]
AppName=JMCampusNetLinker
AppVersion=1.0.0
PrivilegesRequired=admin
DefaultDirName={autopf}\JMCampusNetLinker
DefaultGroupName=JMCampusNetLinker
OutputBaseFilename=JMCampusNetLinker_Setup
Compression=lzma
SolidCompression=yes

[Languages]
Name: "chinesesimp"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "dist\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
Name: "{autoprograms}\JMCampusNetLinker"; Filename: "{app}\JMCampusNetLinker.exe"
Name: "{autodesktop}\JMCampusNetLinker"; Filename: "{app}\JMCampusNetLinker.exe"

[Run]
Filename: "{app}\JMCampusNetLinker.exe"; Description: "JMCampusNetLinker"; Flags: nowait postinstall skipifsilent

[UninstallRun]
Filename: "reg"; Parameters: "delete ""HKCU\Software\Microsoft\Windows\CurrentVersion\Run"" /v JMCampusNetLinker /f"; Flags: runhidden; RunOnceId: "RemoveAutoStart"
