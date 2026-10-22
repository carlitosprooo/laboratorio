; BANKMASTER BY KALOX — Windows x64 installer (Inno Setup 6)
;
; Build:
;   ISCC.exe /DArtefacts="<repo>\build\BankMaster_artefacts\Release" /DOutDir="<repo>\BANKMASTER\dist" BANKMASTER.iss
;
; Installs:
;   VST3        -> C:\Program Files\Common Files\VST3\BANKMASTER.vst3   (standard VST3 location)
;   Standalone  -> C:\Program Files\Kalox Audio\BANKMASTER\BANKMASTER.exe (+ Start menu shortcut)
; The uninstaller removes exactly these files. User presets in %APPDATA%\KALOX\BANKMASTER are kept.
; No other plug-in (e.g. KaloxStart, KaloxSooth) is touched.
;
; Upgrade from 1.0: same AppId, so 2.0 replaces 1.0 in "Apps & features". The 1.0 standalone
; folder (Program Files\KALOX\BANKMASTER), its Start-menu group and its registry key are removed;
; the VST3 bundle is replaced in place (same plug-in IDs: existing projects keep loading it).

#define AppName      "BANKMASTER BY KALOX"
#define AppShortName "BANKMASTER"
#define AppVersion   "2.0.0"
#define AppPublisher "Kalox Audio"
#ifndef Artefacts
  #define Artefacts "..\..\..\build\BankMaster_artefacts\Release"
#endif
#ifndef OutDir
  #define OutDir "..\..\dist"
#endif

[Setup]
AppId={{951C821C-D559-4BEF-BC26-6822C33BB554}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
AppCopyright=(c) 2026 Kalox Audio
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} {#AppVersion} Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
DefaultDirName={autopf}\Kalox Audio\{#AppShortName}
UsePreviousAppDir=no
DisableDirPage=yes
DefaultGroupName=Kalox Audio\{#AppShortName}
UsePreviousGroup=no
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#OutDir}
OutputBaseFilename=BANKMASTER-BY-KALOX-{#AppVersion}-Windows-x64-Setup
SetupIconFile=..\assets\bankmaster.ico
UninstallDisplayIcon={app}\bankmaster.ico
UninstallDisplayName={#AppName}
WizardStyle=modern
WizardImageFile=..\assets\wizard_large_100.bmp,..\assets\wizard_large_200.bmp
WizardSmallImageFile=..\assets\wizard_small_100.bmp,..\assets\wizard_small_200.bmp
WizardImageStretch=no
LicenseFile=..\macos\resources\license.txt
InfoBeforeFile=readme.txt
Compression=lzma2/ultra64
SolidCompression=yes
CloseApplications=yes
SetupLogging=yes
ShowLanguageDialog=auto

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "brazilianportuguese"; MessagesFile: "compiler:Languages\BrazilianPortuguese.isl"

[Types]
Name: "full";     Description: "VST3 plug-in + Standalone application"
Name: "vst3only"; Description: "VST3 plug-in only"
Name: "custom";   Description: "Custom"; Flags: iscustom

[Components]
Name: "vst3";       Description: "VST3 plug-in  (Common Files\VST3\BANKMASTER.vst3)"; Types: full vst3only custom
Name: "standalone"; Description: "Standalone application  (Program Files\Kalox Audio\BANKMASTER)"; Types: full

[Files]
; VST3 bundle (only the bundle produced by the build: binary + moduleinfo.json)
Source: "{#Artefacts}\VST3\BANKMASTER.vst3\*"; DestDir: "{commoncf64}\VST3\BANKMASTER.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; Standalone application
Source: "{#Artefacts}\Standalone\BANKMASTER.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion
; icon used by "Apps & features" and the read-me
Source: "..\assets\bankmaster.ico"; DestDir: "{app}"; Flags: ignoreversion
Source: "readme.txt"; DestDir: "{app}"; DestName: "Read Me.txt"; Flags: ignoreversion

[Icons]
Name: "{group}\BANKMASTER"; Filename: "{app}\BANKMASTER.exe"; IconFilename: "{app}\bankmaster.ico"; Components: standalone
Name: "{group}\Read Me"; Filename: "{app}\Read Me.txt"
Name: "{group}\Uninstall BANKMASTER"; Filename: "{uninstallexe}"; IconFilename: "{app}\bankmaster.ico"

[Registry]
; 1.0 key (vendor was "KALOX"): removed on upgrade
Root: HKLM64; Subkey: "Software\KALOX\BANKMASTER"; ValueType: none; Flags: deletekey noerror
Root: HKLM64; Subkey: "Software\Kalox Audio"; Flags: uninsdeletekeyifempty
Root: HKLM64; Subkey: "Software\Kalox Audio\BANKMASTER"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "Software\Kalox Audio\BANKMASTER"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"
Root: HKLM64; Subkey: "Software\Kalox Audio\BANKMASTER"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"
Root: HKLM64; Subkey: "Software\Kalox Audio\BANKMASTER"; ValueType: string; ValueName: "VST3Path"; ValueData: "{commoncf64}\VST3\BANKMASTER.vst3"; Components: vst3

[InstallDelete]
; leftovers of a 1.0 installation (standalone folder and Start-menu group under "KALOX")
Type: files; Name: "{autopf}\KALOX\BANKMASTER\BANKMASTER.exe"
Type: files; Name: "{autopf}\KALOX\BANKMASTER\bankmaster.ico"
Type: files; Name: "{autopf}\KALOX\BANKMASTER\Read Me.txt"
Type: files; Name: "{autopf}\KALOX\BANKMASTER\unins000.exe"
Type: files; Name: "{autopf}\KALOX\BANKMASTER\unins000.dat"
Type: dirifempty; Name: "{autopf}\KALOX\BANKMASTER"
Type: dirifempty; Name: "{autopf}\KALOX"
Type: filesandordirs; Name: "{autoprograms}\KALOX\BANKMASTER"
Type: dirifempty; Name: "{autoprograms}\KALOX"

[UninstallDelete]
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3\Contents\x86_64-win"
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3\Contents\Resources"
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3\Contents"
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3"
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{autopf}\Kalox Audio"

[Run]
Filename: "{app}\Read Me.txt"; Description: "Open the Read Me"; Flags: postinstall shellexec skipifsilent unchecked

[Code]
// The 1.0 registry parent key "Software\KALOX" is removed only when nothing else uses it.
procedure CurStepChanged(CurStep: TSetupStep);
begin
  if CurStep = ssPostInstall then
    RegDeleteKeyIfEmpty(HKLM64, 'Software\KALOX');
end;
