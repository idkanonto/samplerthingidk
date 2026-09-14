#ifndef AppVersion
  #define AppVersion "3.0.0"
#endif

#define AppName "recompiler.dll"
#define AppPublisher "recompiler.dll"
#define PluginBundle "recompiler.dll.vst3"
#define StandaloneExe "recompiler.dll.exe"

[Setup]
AppId={{CFCC71E5-FA79-4E64-9FE2-289301E58D92}
AppName={#AppName}
AppVersion={#AppVersion}
AppVerName={#AppName} {#AppVersion}
AppPublisher={#AppPublisher}
VersionInfoVersion={#AppVersion}
VersionInfoProductName={#AppName}
VersionInfoDescription={#AppName} Windows installer
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
WizardStyle=modern
Compression=lzma2/ultra64
SolidCompression=yes
OutputDir=..\build\installer
OutputBaseFilename=recompiler-dll-Windows-Setup
UninstallDisplayIcon={app}\{#StandaloneExe}
SetupLogging=yes

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Files]
Source: "..\build\RandomChopSampler_artefacts\Release\VST3\{#PluginBundle}\*"; DestDir: "{commoncf}\VST3\{#PluginBundle}"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\build\RandomChopSampler_artefacts\Release\Standalone\{#StandaloneExe}"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#StandaloneExe}"; WorkingDir: "{app}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#StandaloneExe}"; WorkingDir: "{app}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#StandaloneExe}"; Description: "Launch {#AppName} standalone"; Flags: nowait postinstall skipifsilent
