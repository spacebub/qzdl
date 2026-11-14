; Windows installer, built by the release workflow with
;   iscc /DAppVersion=<version> packaging\qzdl.iss
; ZDL4 is one self-contained binary, so there is nothing here but it, the
; documents beside it in the zip, and a shortcut.

#ifndef AppVersion
  #define AppVersion "0.0"
#endif

#define AppName "ZDL4"
#define AppPublisher "spacebub"
#define AppURL "https://github.com/spacebub/qzdl"

[Setup]
; Never change AppId: an upgrade finds the previous install by it.
AppId={{08129329-D9FB-4306-B325-52658A31E56F}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppPublisherURL={#AppURL}
AppSupportURL={#AppURL}/issues
AppUpdatesURL={#AppURL}/releases
VersionInfoVersion={#AppVersion}

; lowest: {autopf} becomes {localappdata}\Programs, so nothing needs elevation
; and a silent install from winget or choco never meets a UAC prompt.
PrivilegesRequired=lowest
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
UninstallDisplayIcon={app}\ZDL4.exe
UninstallDisplayName={#AppName} {#AppVersion}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

LicenseFile=..\LICENSE
SetupIconFile=..\src\resources\Win32\ico_icon.ico
WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
OutputDir=..\dist
OutputBaseFilename=ZDL4-win-x64-setup
ChangesAssociations=yes

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "zdlassoc"; Description: "Associate .zdl files with ZDL4"; GroupDescription: "File associations:"

[Files]
Source: "..\build\bin\ZDL4.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md";          DestDir: "{app}"; Flags: ignoreversion
Source: "..\CHANGELOG.md";       DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE";            DestDir: "{app}"; Flags: ignoreversion
Source: "..\AUTHORS.md";         DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";    Filename: "{app}\ZDL4.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\ZDL4.exe"; Tasks: desktopicon

; The same keys Associate .zdl files with ZDL4 in Settings writes. -109 is IDI_SAVE, the .zdl icon.
; The uninstall step in [Code] removes them, whichever of the two wrote them.
[Registry]
Root: HKA; Subkey: "Software\Classes\.zdl"; ValueType: string; ValueName: ""; ValueData: "ZDL4.zdl"; Tasks: zdlassoc; Check: Wanted
Root: HKA; Subkey: "Software\Classes\.zdl\OpenWithProgids"; ValueType: string; ValueName: "ZDL4.zdl"; ValueData: ""; Tasks: zdlassoc; Check: Wanted
Root: HKA; Subkey: "Software\Classes\ZDL4.zdl"; ValueType: string; ValueName: ""; ValueData: "ZDL4 launch config"; Tasks: zdlassoc; Check: Wanted
Root: HKA; Subkey: "Software\Classes\ZDL4.zdl\DefaultIcon"; ValueType: string; ValueName: ""; ValueData: """{app}\ZDL4.exe"",-109"; Tasks: zdlassoc; Check: Wanted
Root: HKA; Subkey: "Software\Classes\ZDL4.zdl\shell\open\command"; ValueType: string; ValueName: ""; ValueData: """{app}\ZDL4.exe"" --launch ""%1"""; Tasks: zdlassoc; Check: Wanted

[Run]
Filename: "{app}\ZDL4.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; Profiles and config live in %APPDATA%, never under {app}, so uninstalling
; removes the program and leaves the user's data alone.

[Code]
const
  ProgId = 'ZDL4.zdl';
  Extension = 'Software\Classes\.zdl';
  Previous = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\{08129329-D9FB-4306-B325-52658A31E56F}_is1';

var
  TurnedOff: Boolean;

// An upgrade remembers the task, so an association turned off in Settings since would come back.
function InitializeSetup(): Boolean;
var
  Tasks: String;
begin
  TurnedOff := RegQueryStringValue(HKA, Previous, 'Inno Setup: Selected Tasks', Tasks)
    and (Pos('zdlassoc', Tasks) > 0) and not RegKeyExists(HKA, 'Software\Classes\' + ProgId);
  Result := True;
end;

// Shown the choice, the user's tick decides.
procedure CurPageChanged(CurPageID: Integer);
begin
  if (CurPageID = wpSelectTasks) and TurnedOff then
  begin
    WizardSelectTasks('!zdlassoc');
    TurnedOff := False;
  end;
end;

function Wanted(): Boolean;
begin
  Result := not TurnedOff;
end;

// Left alone when another copy of ZDL4, such as a portable one, holds the association.
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Value: String;
begin
  if CurUninstallStep <> usPostUninstall then
    Exit;

  if RegQueryStringValue(HKA, 'Software\Classes\' + ProgId + '\shell\open\command', '', Value)
    and (Pos(Lowercase(ExpandConstant('"{app}\ZDL4.exe"')), Lowercase(Value)) <> 1) then
    Exit;

  RegDeleteKeyIncludingSubkeys(HKA, 'Software\Classes\' + ProgId);
  RegDeleteValue(HKA, Extension + '\OpenWithProgids', ProgId);

  if RegQueryStringValue(HKA, Extension, '', Value) and (CompareText(Value, ProgId) = 0) then
    RegDeleteValue(HKA, Extension, '');

  RegDeleteKeyIfEmpty(HKA, Extension + '\OpenWithProgids');
  RegDeleteKeyIfEmpty(HKA, Extension);
end;
