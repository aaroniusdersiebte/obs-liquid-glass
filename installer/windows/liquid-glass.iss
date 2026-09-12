; Inno Setup script for Liquid Glass (obs-liquid-glass).
; Not part of the upstream obs-plugintemplate scaffold -- the template only
; zips the Windows build. This adds a real installer on top, invoked from
; the windows-build job in .github/workflows/build-project.yaml via ISCC.exe
; with /DMyAppVersion=<version> /DSourceDir=<release/<Config> path>.
;
; DefaultDirName auto-detects the user's actual OBS Studio install directory
; from the registry key OBS's own installer writes (HKLM\SOFTWARE\OBS Studio,
; default value) -- falls back to {autopf}\obs-studio if that key is absent
; (e.g. a portable OBS install). The user can still browse to a different
; folder on the wizard's directory page.

#ifndef MyAppVersion
  #define MyAppVersion "0.0.0"
#endif
#ifndef SourceDir
  #define SourceDir "..\..\release\RelWithDebInfo"
#endif

#define MyAppName "Liquid Glass for OBS"
#define MyAppPublisher "Aaronius"
#define MyAppURL "https://aaronius.com"

[Setup]
AppId={{6C4B9E2A-2E0C-4B7B-8C1E-6D6C7B9E2A6C}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={code:GetOBSDir}
DisableDirPage=no
DisableProgramGroupPage=yes
DisableReadyPage=yes
DirExistsWarning=no
OutputDir=.
OutputBaseFilename=obs-liquid-glass-{#MyAppVersion}-windows-x64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
WizardStyle=modern
UninstallDisplayName={#MyAppName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "german"; MessagesFile: "compiler:Languages\German.isl"

[Files]
Source: "{#SourceDir}\obs-liquid-glass\bin\64bit\obs-liquid-glass.dll"; DestDir: "{app}\obs-plugins\64bit"; Flags: ignoreversion
Source: "{#SourceDir}\obs-liquid-glass\data\*"; DestDir: "{app}\data\obs-plugins\obs-liquid-glass"; Flags: ignoreversion recursesubdirs createallsubdirs

[Code]
function GetOBSDir(Param: string): string;
var
  InstallDir: string;
begin
  Result := ExpandConstant('{autopf}\obs-studio');
  if RegQueryStringValue(HKLM, 'SOFTWARE\OBS Studio', '', InstallDir) then
  begin
    if DirExists(InstallDir) then
      Result := InstallDir;
  end;
end;
