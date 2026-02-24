@echo off
chcp 65001 >nul
title MinecraftMix Installer - Otomatik Kurulum ve Derleyici

echo ==========================================================
echo  MinecraftMix Ön Sürüm - GitHub Otomatik Derleyici
echo ==========================================================
echo.

:: 1. Python Kontrolü
python --version >nul 2>&1
if %errorlevel% neq 0 (
    echo [HATA] Sisteminizde Python bulunamadi!
    echo Lutfen https://www.python.org adresinden Python 3 kurun.
    echo Kurulum sirasinda "Add Python to PATH" secenegini isaretlemeyi unutmayin!
    pause
    exit /b
)
echo [BiLGi] Python basariyla algilandi.

:: 2. Gerekli kütüphaneleri kur
echo [BiLGi] PyInstaller modulu indiriliyor/kontrol ediliyor...
pip install pyinstaller >nul 2>&1

:: 3. Kendi GitHub Adresinizden Dosyaları Çekme
:: NOT: KULLANICI_ADI ve REPO_ADI kisimlarini kendi GitHub linkinize gore duzenleyin!
set GITHUB_BASE_URL=https://raw.githubusercontent.com/Beko-soft/mixlauncher/master

echo [BiLGi] Kaynak dosyalar GitHub'dan indiriliyor...
if not exist "pre_installer.py" (
    curl -sOL "%GITHUB_BASE_URL%/pre_installer.py"
)
if not exist "minecraftmix_pack.zip" (
    curl -sOL "%GITHUB_BASE_URL%/minecraftmix_pack.zip"
)

if not exist "pre_installer.py" (
    echo [HATA] pre_installer.py indirilemedi! Lutfen bat dosyasi icindeki GITHUB url'sini duzenleyin.
    pause
    exit /b
)

if not exist "minecraftmix_pack.zip" (
    echo [UYARI] minecraftmix_pack.zip bulunamadi! (Zip bos olsa bile script patlamaz ama icerik eksik olur.)
)

:: 4. Derleme İsşemi
echo [BiLGi] Windows EXE dosyasi derleniyor, bu islem biraz surebilir...
pyinstaller --noconfirm --onefile --windowed --add-data "minecraftmix_pack.zip;." pre_installer.py

if exist "dist\pre_installer.exe" (
    echo [BiLGi] Derleme basariyla tamamlandi!
    
    :: 5. Masaüstü Kısayolu Oluşturma
    echo [BiLGi] Masaustu kisayolu olusturuluyor...
    set SCRIPT="%TEMP%\CreateShortcut.vbs"
    echo Set oWS = WScript.CreateObject("WScript.Shell") > %SCRIPT%
    echo sLinkFile = "%USERPROFILE%\Desktop\MinecraftMix On Surum.lnk" >> %SCRIPT%
    echo Set oLink = oWS.CreateShortcut(sLinkFile) >> %SCRIPT%
    echo oLink.TargetPath = "%CD%\dist\pre_installer.exe" >> %SCRIPT%
    echo oLink.WorkingDirectory = "%CD%\dist" >> %SCRIPT%
    echo oLink.Description = "MinecraftMix On Surum Kurulum Araci" >> %SCRIPT%
    echo oLink.Save >> %SCRIPT%
    cscript /nologo %SCRIPT%
    del %SCRIPT%

    echo.
    echo ==========================================================
    echo  ISLEM TAMAMLANDI!
    echo  Masaustunuzdeki "MinecraftMix On Surum" kisayoluna
    echo  tiklayarak araci calistirabilirsiniz.
    echo ==========================================================
) else (
    echo [HATA] EXE derleme islemi basarisiz oldu. Lutfen konsol uyarilarini inceleyin.
)

pause
