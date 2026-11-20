#define PluginSource GetEnv("MAKOSIZERX_VST3_PATH")
#define ProductVersion GetEnv("MAKOSIZERX_VERSION")

#if PluginSource == ""
  #error "Set MAKOSIZERX_VST3_PATH to the built MakosizerX.vst3 bundle."
#endif

#if ProductVersion == ""
  #define ProductVersion "1.0.0"
#endif

[Setup]
AppId={{6B7F4348-6091-45B5-B081-1AD0472A64D9}
AppName=MakosizerX
AppVersion={#ProductVersion}
AppPublisher=MakosizerX
DefaultDirName={commoncf64}\VST3
DisableDirPage=yes
DisableProgramGroupPage=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
PrivilegesRequired=admin
OutputBaseFilename=MakosizerX-Setup
UninstallDisplayName=MakosizerX VST3
Uninstallable=yes
WizardStyle=modern

[Files]
Source: "{#PluginSource}\*"; DestDir: "{commoncf64}\VST3\MakosizerX.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
