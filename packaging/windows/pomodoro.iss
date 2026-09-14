; Inno Setup script for the Windows installer.
;
; Installs entirely inside the user's own profile, so it never asks for an administrator
; and never touches Program Files. That is what PrivilegesRequired=lowest buys: with it,
; {autopf} resolves to %LOCALAPPDATA%\Programs and {autoprograms} to the per-user Start
; menu, so an ordinary account can install, update and remove the app unaided.
;
; Built by CI with the version passed in:
;   iscc /DAppVersion=1.2.3 /DSourceDir=..\..\dist\pomodoro packaging\windows\pomodoro.iss

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif

#ifndef SourceDir
  #define SourceDir "..\..\dist\pomodoro"
#endif

#define AppName "Pomodoro"
#define AppPublisher "elhiba"
#define AppExeName "pomodoro.exe"
#define AppUserModelID "elhiba.pomodoro"

[Setup]
; Never change AppId: it is what lets a new version replace the old one rather than
; installing alongside it, and what ties the entry in Apps & features to this program.
AppId={{9F1C4A2E-7B83-4D56-9C21-0E5A8D3B6F14}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppSupportURL=https://github.com/elhiba/pomodoro
AppUpdatesURL=https://github.com/elhiba/pomodoro/releases

; No administrator, ever.
PrivilegesRequired=lowest
PrivilegesRequiredOverridesAllowed=dialog

DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
DisableDirPage=auto

OutputDir=..\..\dist
OutputBaseFilename=pomodoro-{#AppVersion}-windows-x64-setup
SetupIconFile=..\..\assets\icons\pomodoro.ico
UninstallDisplayIcon={app}\{#AppExeName}
UninstallDisplayName={#AppName}

WizardStyle=modern
Compression=lzma2/max
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
; The whole windeployqt folder: the executable, the Qt runtime, the QML module and the
; FFmpeg libraries the multimedia backend needs.
Source: "{#SourceDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs

[Icons]
; AppUserModelID has to match the id the app claims at start-up. It is what lets Windows
; put a name and an icon on the media flyout entry instead of calling it "Unknown app",
; and the file name matches the one the app would otherwise write for itself, so an
; installed copy never ends up with two Start menu entries.
Name: "{autoprograms}\{#AppName}"; Filename: "{app}\{#AppExeName}"; AppUserModelID: "{#AppUserModelID}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; AppUserModelID: "{#AppUserModelID}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#AppExeName}"; Description: "Launch {#AppName}"; Flags: nowait postinstall skipifsilent

[UninstallDelete]
; Settings live in the registry and the session history under AppData; both are left
; alone on uninstall so reinstalling does not lose a streak. Only the unpacked caches
; this install created are cleared.
Type: filesandordirs; Name: "{localappdata}\{#AppPublisher}\pomodoro\cache"
