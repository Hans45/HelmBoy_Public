#define MyAppVersion "1.1.0"

[Setup]
AppName=HelmBoy Synthesizer
AppVersion={#MyAppVersion}
AppPublisher=Marc Scheffer
AppPublisherURL=https://github.com/Hans45/HelmBoy
AppSupportURL=https://github.com/Hans45/HelmBoy/issues
AppUpdatesURL=https://github.com/Hans45/HelmBoy/releases
DefaultDirName={autopf}\HelmBoy
DefaultGroupName=HelmBoy Synthesizer
AllowNoIcons=yes
LicenseFile=COPYING
InfoBeforeFile=README.md
ShowLanguageDialog=yes
DisableWelcomePage=no
UninstallDisplayIcon={app}\HelmBoy.exe
OutputDir=installer
OutputBaseFilename=helmBoy-setup-{#MyAppVersion}
SetupIconFile=images\inno setup graphics\helmBoy.ico
Compression=lzma2/ultra64
SolidCompression=yes
WizardImageFile=images\inno setup graphics\helmBoy_wizard.png
WizardStyle=modern dynamic windows11
ArchitecturesInstallIn64BitMode=x64compatible
ArchitecturesAllowed=x64compatible
PrivilegesRequired=lowest

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Types]
Name: "full"; Description: "Installation complte"
Name: "custom"; Description: "Installation personnalise"; Flags: iscustom

[Components]
Name: "standalone"; Description: "Standalone Application"; Types: full custom
Name: "vst3"; Description: "VST3 Plugin"; Types: full custom
Name: "lv2"; Description: "LV2 Plugin"; Types: full custom
Name: "patches"; Description: "Patches Banks"; Types: full custom

[Dirs]
Name: "{userdocs}\HelmBoy"; Flags: uninsneveruninstall; Check: not IsAdminInstallMode
Name: "{userdocs}\HelmBoy\Patches"; Flags: uninsneveruninstall; Check: not IsAdminInstallMode
Name: "{commondocs}\HelmBoy\Patches"; Flags: uninsneveruninstall

[Files]
; Application standalone (autonome - aucun DLL externe requis)
Source: "build\HelmBoyStandalone_artefacts\Release\HelmBoy.exe"; DestDir: "{app}"; Components: standalone; Flags: ignoreversion

; Plugin VST3
Source: "build\HelmBoyPlugin_artefacts\Release\VST3\HelmBoy.vst3\*"; DestDir: "{commoncf}\VST3\Marc Scheffer\HelmBoy.vst3"; Components: vst3; Flags: ignoreversion recursesubdirs createallsubdirs
; Plugin LV2
Source: "build\HelmBoyPlugin_artefacts\Release\LV2\HelmBoy.lv2\*"; DestDir: "{commoncf}\LV2\Marc Scheffer"; Components: lv2; Flags: ignoreversion recursesubdirs createallsubdirs

; Documentation et licence
Source: "README.md"; DestDir: "{commondocs}\HelmBoy"; Flags: ignoreversion; Check: IsAdminInstallMode
Source: "COPYING"; DestDir: "{commondocs}\HelmBoy"; Flags: ignoreversion; Check: IsAdminInstallMode
Source: "README.md"; DestDir: "{userdocs}\HelmBoy"; Flags: ignoreversion; Check: not IsAdminInstallMode
Source: "COPYING"; DestDir: "{userdocs}\HelmBoy"; Flags: ignoreversion; Check: not IsAdminInstallMode

; Factory Presets (repertoire public pour les plugins)
Source: "patches\*"; DestDir: "{commondocs}\HelmBoy\Patches\"; Components: patches; Flags: ignoreversion recursesubdirs createallsubdirs; Check: IsAdminInstallMode
Source: "patches\*"; DestDir: "{userdocs}\HelmBoy\Patches\"; Components: patches; Flags: ignoreversion recursesubdirs createallsubdirs; Check: not IsAdminInstallMode

[Icons]
Name: "{group}\HelmBoy Synthesizer"; Filename: "{app}\HelmBoy.exe"; Components: standalone
Name: "{group}\HelmBoy Readme"; Filename: "{commondocs}\HelmBoy\README.md"
Name: "{group}\HelmBoy License"; Filename: "{commondocs}\HelmBoy\COPYING"
Name: "{group}\{cm:UninstallProgram,HelmBoy}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\HelmBoy Synthesizer"; Filename: "{app}\HelmBoy.exe"; Components: standalone; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked; Components: standalone

[Run]
Filename: "{app}\HelmBoy.exe"; Description: "{cm:LaunchProgram,HelmBoy Synthesizer}"; Flags: nowait postinstall skipifsilent; Components: standalone