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
Name: "installprerelease"; Description: "Minecraft 1.21.4'ü Bilgisayara İndir (İnternet gerektirmez)"; GroupDescription: "Offline Yardımcı Araçlar:"; Flags: checkedonce

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
Source: "offline_bundle\versions\1.21.4\*"; DestDir: "{localappdata}\.minecraftmix\versions\1.21.4"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "offline_bundle\libraries\*"; DestDir: "{localappdata}\.minecraftmix\libraries"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "offline_bundle\assets\*"; DestDir: "{localappdata}\.minecraftmix\assets"; Tasks: installprerelease; Flags: ignoreversion recursesubdirs createallsubdirs


; === Offline Profil Yapılandırması ===
Source: "offline_bundle\profiles.json"; DestDir: "{localappdata}\.minecraftmix"; Tasks: installprerelease; Flags: ignoreversion

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
    McDir := ExpandConstant('{localappdata}\.minecraftmix');

    { .minecraftmix ana dizinlerini oluştur }
    ForceDirectories(McDir + '\versions');
    ForceDirectories(McDir + '\libraries');
    ForceDirectories(McDir + '\assets\indexes');
    ForceDirectories(McDir + '\assets\objects');
    ForceDirectories(McDir + '\profiles');

    { Ön sürüm etkinleştirildiyse profili oluştur }
    if IsTaskSelected('installprerelease') then begin
      ProfilePath := McDir + '\profiles.json';
      if not FileExists(ProfilePath) then begin
        ProfileContent := '[{"name":"Vanilla 1.21.4","gameVersion":"1.21.4","loader":"vanilla","modsPath":""}]';
        SaveStringToFile(ProfilePath, ProfileContent, False);
      end;

      MsgBox('Minecraft 1.21.4 başarıyla kuruldu!' + #13#10 +
             'İnternet olmadan "Vanilla 1.21.4" profiliyle oynayabilirsiniz.',
             mbInformation, MB_OK);
    end;
  end;
end;
