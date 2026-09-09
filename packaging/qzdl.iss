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

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "..\build\bin\ZDL4.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\README.md";          DestDir: "{app}"; Flags: ignoreversion
Source: "..\CHANGELOG";          DestDir: "{app}"; Flags: ignoreversion
Source: "..\LICENSE";            DestDir: "{app}"; Flags: ignoreversion
Source: "..\AUTHORS";            DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}";    Filename: "{app}\ZDL4.exe"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\ZDL4.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\ZDL4.exe"; Description: "{cm:LaunchProgram,{#AppName}}"; Flags: nowait postinstall skipifsilent

; Profiles and config live in %APPDATA%, never under {app}, so uninstalling
; removes the program and leaves the user's data alone.
