; BANKMASTER by KALOX — Windows x64 installer (Inno Setup 6)
;
; Build:
;   ISCC.exe /DArtefacts="<repo>\build\BankMaster_artefacts\Release" /DOutDir="<repo>\BANKMASTER\dist" BANKMASTER.iss
;
; Installs:
;   VST3        -> C:\Program Files\Common Files\VST3\BANKMASTER.vst3   (standard VST3 location)
;   Standalone  -> C:\Program Files\KALOX\BANKMASTER\BANKMASTER.exe      (+ Start menu shortcut)
; The uninstaller removes exactly these files. User presets in %APPDATA%\KALOX\BANKMASTER are kept.
; No other plug-in (e.g. KaloxStart, KaloxSooth) is touched.

#define AppName      "BANKMASTER by KALOX"
#define AppShortName "BANKMASTER"
#define AppVersion   "1.0.0"
#define AppPublisher "KALOX"
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
AppCopyright=(c) 2026 KALOX
VersionInfoVersion={#AppVersion}.0
VersionInfoCompany={#AppPublisher}
VersionInfoDescription={#AppName} {#AppVersion} Setup
VersionInfoProductName={#AppName}
VersionInfoProductVersion={#AppVersion}
DefaultDirName={autopf}\KALOX\{#AppShortName}
DisableDirPage=yes
DefaultGroupName=KALOX\{#AppShortName}
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
PrivilegesRequired=admin
OutputDir={#OutDir}
OutputBaseFilename=BANKMASTER-by-KALOX-{#AppVersion}-Windows-x64-Setup
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
Name: "standalone"; Description: "Standalone application  (Program Files\KALOX\BANKMASTER)"; Types: full

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
Root: HKLM64; Subkey: "Software\KALOX"; Flags: uninsdeletekeyifempty
Root: HKLM64; Subkey: "Software\KALOX\BANKMASTER"; Flags: uninsdeletekey
Root: HKLM64; Subkey: "Software\KALOX\BANKMASTER"; ValueType: string; ValueName: "Version"; ValueData: "{#AppVersion}"
Root: HKLM64; Subkey: "Software\KALOX\BANKMASTER"; ValueType: string; ValueName: "InstallDir"; ValueData: "{app}"
Root: HKLM64; Subkey: "Software\KALOX\BANKMASTER"; ValueType: string; ValueName: "VST3Path"; ValueData: "{commoncf64}\VST3\BANKMASTER.vst3"; Components: vst3

[UninstallDelete]
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3\Contents\x86_64-win"
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3\Contents\Resources"
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3\Contents"
Type: dirifempty; Name: "{commoncf64}\VST3\BANKMASTER.vst3"
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{autopf}\KALOX"

[Run]
Filename: "{app}\Read Me.txt"; Description: "Open the Read Me"; Flags: postinstall shellexec skipifsilent unchecked
