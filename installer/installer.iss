#ifndef MyAppName
  #define MyAppName "Replay Countdown"
#endif
#ifndef MyAppVersion
  #define MyAppVersion "1.0.0"
#endif
#ifndef MySourceDir
  #define MySourceDir "..\release\RelWithDebInfo"
#endif
#ifndef MyOutputDir
  #define MyOutputDir "..\release"
#endif
#ifndef MyOutputBaseFilename
  #define MyOutputBaseFilename "obs-replay-countdown-1.0.0-windows-x64-Installer"
#endif

[Setup]
AppId={{8F4E2B19-7C3A-4D91-A5E2-91B4C6D8021F}
AppName={#MyAppName} for OBS Studio
AppVersion={#MyAppVersion}
AppPublisher=GameRec
AppPublisherURL=https://ko-fi.com/gamerec
AppSupportURL=https://github.com/trx90th/obs-replay-countdown
DefaultDirName={code:GetObsInstallDir}
DisableProgramGroupPage=yes
OutputDir={#MyOutputDir}
OutputBaseFilename={#MyOutputBaseFilename}
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "polish"; MessagesFile: "compiler:Languages\Polish.isl"

[Files]
Source: "{#MySourceDir}\obs-plugins\*"; DestDir: "{app}\obs-plugins"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "{#MySourceDir}\data\*"; DestDir: "{app}\data"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
function GetObsInstallDir(Param: String): String;
var
  ObsPath: String;
begin
  if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\OBS Studio', '', ObsPath) and (ObsPath <> '') then
    Result := ObsPath
  else if RegQueryStringValue(HKEY_LOCAL_MACHINE, 'SOFTWARE\WOW6432Node\OBS Studio', '', ObsPath) and (ObsPath <> '') then
    Result := ObsPath
  else
    Result := ExpandConstant('{autopf}\obs-studio');
end;