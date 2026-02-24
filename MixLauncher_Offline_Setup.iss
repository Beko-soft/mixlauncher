; ═══════════════════════════════════════════════════════════
;  MixLauncher v2.0 — Offline Installer (Inno Setup)
;  Fabric 1.21.4 Ön Sürüm Dahil
; ═══════════════════════════════════════════════════════════

#define AppName "MixLauncher"
#define AppVersion "2.0.0"
#define AppPublisher "MixCraft"
#define AppExeName "MixLauncher.exe"

[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#AppName}
AppVersion={#AppVersion}
AppPublisher={#AppPublisher}
DefaultDirName={autopf}\{#AppName}
DefaultGroupName={#AppName}
UninstallDisplayIcon={app}\{#AppExeName}
OutputDir=installer_output
OutputBaseFilename=MixLauncher_v2.0_Offline_Setup
Compression=lzma2/ultra64
SolidCompression=yes
PrivilegesRequired=lowest
WizardStyle=modern
SetupIconFile=assets\icon.ico
DisableProgramGroupPage=yes

; ═══════════════════════════════════════════════════════════
;  KURULUM GÖREV SEÇENEKLERİ
; ═══════════════════════════════════════════════════════════
[Tasks]
Name: "desktopicon"; Description: "Masaüstü kısayolu oluştur"; GroupDescription: "Ek Görevler:"
Name: "installprerelease"; Description: "Ön Sürümü Etkinleştir (Fabric 1.21.4 — İnternet gerektirmez)"; GroupDescription: "Offline Ön Sürüm:"; Flags: checkedonce

; ═══════════════════════════════════════════════════════════
;  DOSYA LİSTESİ
; ═══════════════════════════════════════════════════════════
[Files]
; === Ana uygulama ve Qt bağımlılıkları ===
Source: "build\Release\{#AppExeName}"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs
Source: "build\Release\platforms\*"; DestDir: "{app}\platforms"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "build\Release\styles\*"; DestDir: "{app}\styles"; Flags: ignoreversion recursesubdirs createallsubdirs

; === Offline Ön Sürüm: Minecraft 1.21.4 Vanilla dosyaları ===
; (Kullanıcı "Ön Sürümü Etkinleştir" seçeneğini işaretlediğinde kopyalanır)
Source: "offline_bundle\versions\1.21.4\*"; DestDir: "{localappdata}\.mixcrafter\versions\1.21.4"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "offline_bundle\libraries\*"; DestDir: "{localappdata}\.mixcrafter\libraries"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "offline_bundle\assets\*"; DestDir: "{localappdata}\.mixcrafter\assets"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs

; === Offline Ön Sürüm: Fabric Loader dosyaları ===
Source: "offline_bundle\versions\fabric-loader-*\*"; DestDir: "{localappdata}\.mixcrafter\versions\fabric-loader-{code:GetFabricVersionDir}"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs

; === Offline Profil Yapılandırması ===
Source: "offline_bundle\profiles.json"; DestDir: "{localappdata}\.mixcrafter"; Tasks: installprerelease; Flags: ignoreversion

; ═══════════════════════════════════════════════════════════
;  BAŞLAT MENÜSÜ & MASAÜSTÜ
; ═══════════════════════════════════════════════════════════
[Icons]
Name: "{group}\{#AppName}"; Filename: "{app}\{#AppExeName}"
Name: "{autodesktop}\{#AppName}"; Filename: "{app}\{#AppExeName}"; Tasks: desktopicon

; ═══════════════════════════════════════════════════════════
;  KURULUM SONRASI ÇALIŞTIR
; ═══════════════════════════════════════════════════════════
[Run]
Filename: "{app}\{#AppExeName}"; Description: "{#AppName} uygulamasını başlat"; Flags: nowait postinstall skipifsilent

; ═══════════════════════════════════════════════════════════
;  PASCAL SCRIPT KOD BLOĞU
; ═══════════════════════════════════════════════════════════
[Code]
function GetFabricVersionDir(Param: String): String;
begin
  { Offline bundle'daki Fabric loader klasör adını döndür }
  Result := '';
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  McDir: String;
  ProfilePath: String;
  ProfileContent: String;
begin
  if CurStep = ssPostInstall then begin
    McDir := ExpandConstant('{localappdata}\.mixcrafter');

    { .mixcrafter ana dizinlerini oluştur }
    ForceDirectories(McDir + '\versions');
    ForceDirectories(McDir + '\libraries');
    ForceDirectories(McDir + '\assets\indexes');
    ForceDirectories(McDir + '\assets\objects');
    ForceDirectories(McDir + '\profiles');

    { Ön sürüm etkinleştirildiyse profili oluştur }
    if IsTaskSelected('installprerelease') then begin
      ProfilePath := McDir + '\profiles.json';
      if not FileExists(ProfilePath) then begin
        ProfileContent := '[{"name":"Fabric 1.21.4","gameVersion":"1.21.4","loader":"fabric","modsPath":"' + McDir + '/profiles/Fabric 1.21.4/mods"}]';
        SaveStringToFile(ProfilePath, ProfileContent, False);
      end;

      { Mods klasörünü oluştur }
      ForceDirectories(McDir + '\profiles\Fabric 1.21.4\mods');

      MsgBox('Ön Sürüm (Fabric 1.21.4) başarıyla kuruldu!' + #13#10 +
             'İnternet olmadan "Fabric 1.21.4" profiliyle oynayabilirsiniz.' + #13#10 + #13#10 +
             'Mods klasörü: ' + McDir + '\profiles\Fabric 1.21.4\mods',
             mbInformation, MB_OK);
    end;
  end;
end;
