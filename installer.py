#!/usr/bin/env python3
# ═══════════════════════════════════════════════════════════════
#  MixLauncher v2.0 — Grafik Arayüzlü Kurulum Sihirbazı
#  Fabric 1.21.4 Ön Sürüm Otomatik İndirici & Kurucu
# ═══════════════════════════════════════════════════════════════

import tkinter as tk
from tkinter import ttk, messagebox
import threading
import json
import os
import sys
import subprocess
import urllib.request
import urllib.error
import ssl
import hashlib
from pathlib import Path

# ── SSL bağlantı ayarı ──
ssl_ctx = ssl.create_default_context()
ssl_ctx.check_hostname = False
ssl_ctx.verify_mode = ssl.CERT_NONE

# ── Sabitler ──
GAME_VERSION = "1.21.4"
FABRIC_META = "https://meta.fabricmc.net/v2"
MOJANG_MANIFEST = "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"
UA = "MixLauncher-Installer/2.0"

HOME = Path.home()
if sys.platform == "win32":
    MC_DIR = Path(os.environ.get("LOCALAPPDATA", HOME)) / ".mixcrafter"
else:
    MC_DIR = HOME / ".mixcrafter"

SCRIPT_DIR = Path(__file__).parent.resolve()
LAUNCHER_EXE = SCRIPT_DIR / "build" / "MixLauncher"
if sys.platform == "win32":
    LAUNCHER_EXE = SCRIPT_DIR / "build" / "Release" / "MixLauncher.exe"


# ═══════════════════════════════════════════════════════════════
#  Renk Paleti & Tema
# ═══════════════════════════════════════════════════════════════
COLORS = {
    "bg":           "#1a1a2e",
    "bg_light":     "#16213e",
    "card":         "#0f3460",
    "accent":       "#e94560",
    "accent_hover": "#ff6b81",
    "green":        "#2ecc71",
    "green_dark":   "#27ae60",
    "text":         "#eaeaea",
    "text_dim":     "#7f8c8d",
    "border":       "#2c3e50",
    "progress_bg":  "#2c3e50",
    "progress_fg":  "#3498db",
    "white":        "#ffffff",
    "warn":         "#f39c12",
}


# ═══════════════════════════════════════════════════════════════
#  HTTP Yardımcı Fonksiyonları
# ═══════════════════════════════════════════════════════════════
def http_get_json(url):
    """JSON döndüren GET isteği."""
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, context=ssl_ctx, timeout=30) as resp:
        return json.loads(resp.read().decode())


def http_download(url, dest_path, progress_callback=None):
    """Dosya indir, opsiyonel progress callback."""
    dest_path = Path(dest_path)
    dest_path.parent.mkdir(parents=True, exist_ok=True)

    req = urllib.request.Request(url, headers={"User-Agent": UA})
    with urllib.request.urlopen(req, context=ssl_ctx, timeout=60) as resp:
        total = int(resp.headers.get("Content-Length", 0))
        downloaded = 0
        chunk_size = 65536

        with open(dest_path, "wb") as f:
            while True:
                chunk = resp.read(chunk_size)
                if not chunk:
                    break
                f.write(chunk)
                downloaded += len(chunk)
                if progress_callback and total > 0:
                    progress_callback(downloaded, total)

    return dest_path


def sha1_file(path):
    """Dosyanın SHA-1 hash'ini hesapla."""
    h = hashlib.sha1()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def maven_to_path(name):
    """Maven koordinatını dosya yoluna çevir."""
    parts = name.split(":")
    if len(parts) < 3:
        return None
    group = parts[0].replace(".", "/")
    artifact = parts[1]
    version = parts[2]
    classifier = f"-{parts[3]}" if len(parts) > 3 else ""
    return f"{group}/{artifact}/{version}/{artifact}-{version}{classifier}.jar"


# ═══════════════════════════════════════════════════════════════
#  KURULUM MOTORU
# ═══════════════════════════════════════════════════════════════
class InstallEngine:
    """Minecraft + Fabric kurulum mantığı."""

    def __init__(self, mc_dir, on_log, on_progress, on_status):
        self.mc_dir = Path(mc_dir)
        self.on_log = on_log
        self.on_progress = on_progress
        self.on_status = on_status

    def log(self, msg):
        self.on_log(msg)

    def run_full_install(self):
        """Tam kurulum: Vanilla 1.21.4 + Fabric + Profil"""
        try:
            self.on_progress(0)
            self.on_status("Klasörler oluşturuluyor...")
            self._create_dirs()

            self.on_progress(5)
            self.on_status("Minecraft 1.21.4 sürüm bilgisi indiriliyor...")
            ver_json = self._download_version_json()
            if not ver_json:
                return False

            self.on_progress(10)
            self.on_status("Client JAR indiriliyor...")
            self._download_client_jar(ver_json)

            self.on_progress(20)
            self.on_status("Kütüphaneler indiriliyor...")
            self._download_libraries(ver_json)

            self.on_progress(50)
            self.on_status("Asset dosyaları indiriliyor...")
            self._download_assets(ver_json)

            self.on_progress(75)
            self.on_status("Fabric Loader kuruluyor...")
            self._install_fabric()

            self.on_progress(90)
            self.on_status("Profil oluşturuluyor...")
            self._create_profile()

            self.on_progress(100)
            self.on_status("✓ Kurulum Tamamlandı!")
            self.log("═" * 50)
            self.log("  KURULUM BAŞARIYLA TAMAMLANDI!")
            self.log(f"  Dizin: {self.mc_dir}")
            self.log("═" * 50)
            return True

        except Exception as e:
            self.log(f"HATA: {e}")
            self.on_status(f"HATA: {e}")
            return False

    def _create_dirs(self):
        for sub in ["versions", "libraries", "assets/indexes",
                     "assets/objects", "profiles"]:
            (self.mc_dir / sub).mkdir(parents=True, exist_ok=True)
        self.log("✓ Klasörler oluşturuldu")

    def _download_version_json(self):
        self.log("Mojang manifest indiriliyor...")
        manifest = http_get_json(MOJANG_MANIFEST)

        ver_url = None
        for v in manifest.get("versions", []):
            if v["id"] == GAME_VERSION:
                ver_url = v["url"]
                break

        if not ver_url:
            self.log(f"HATA: {GAME_VERSION} sürümü bulunamadı!")
            return None

        self.log(f"Sürüm JSON indiriliyor: {GAME_VERSION}")
        ver_json = http_get_json(ver_url)

        # JSON'u kaydet
        ver_dir = self.mc_dir / "versions" / GAME_VERSION
        ver_dir.mkdir(parents=True, exist_ok=True)
        with open(ver_dir / f"{GAME_VERSION}.json", "w") as f:
            json.dump(ver_json, f, indent=2)
        self.log(f"✓ {GAME_VERSION}.json kaydedildi")

        return ver_json

    def _download_client_jar(self, ver_json):
        dl = ver_json.get("downloads", {}).get("client", {})
        url = dl.get("url", "")
        sha1 = dl.get("sha1", "")
        dest = self.mc_dir / "versions" / GAME_VERSION / f"{GAME_VERSION}.jar"

        if dest.exists() and sha1 and sha1_file(dest) == sha1:
            self.log("✓ Client JAR zaten mevcut (hash doğru)")
            return

        self.log(f"Client JAR indiriliyor ({dl.get('size', 0) // 1024 // 1024} MB)...")
        http_download(url, dest, lambda d, t: self.on_status(
            f"Client JAR: {d * 100 // t}%"))
        self.log("✓ Client JAR indirildi")

    def _download_libraries(self, ver_json):
        libs = ver_json.get("libraries", [])
        total = len(libs)
        downloaded = 0
        skipped = 0

        for i, lib in enumerate(libs):
            # OS filtresi
            if "rules" in lib:
                allowed = False
                for rule in lib["rules"]:
                    action = rule.get("action", "allow")
                    if "os" in rule:
                        os_name = rule["os"].get("name", "")
                        current_os = "linux" if sys.platform.startswith("linux") else (
                            "windows" if sys.platform == "win32" else "osx")
                        if os_name == current_os and action == "allow":
                            allowed = True
                        elif os_name == current_os and action == "disallow":
                            allowed = False
                    else:
                        allowed = (action == "allow")
                if not allowed:
                    skipped += 1
                    continue

            artifact = lib.get("downloads", {}).get("artifact")
            if not artifact:
                continue

            url = artifact.get("url", "")
            path = artifact.get("path", "")
            sha1 = artifact.get("sha1", "")
            dest = self.mc_dir / "libraries" / path

            if dest.exists() and sha1 and sha1_file(dest) == sha1:
                skipped += 1
                continue

            if url:
                try:
                    http_download(url, dest)
                    downloaded += 1
                except Exception as e:
                    self.log(f"  ⚠ Kütüphane indirilemedi: {path} — {e}")

            # İlerleme
            pct = 20 + int((i / max(total, 1)) * 30)
            self.on_progress(pct)
            self.on_status(f"Kütüphaneler: {i + 1}/{total}")

        self.log(f"✓ Kütüphaneler: {downloaded} indirildi, {skipped} atlandı")

    def _download_assets(self, ver_json):
        ai = ver_json.get("assetIndex", {})
        ai_url = ai.get("url", "")
        ai_id = ai.get("id", "")
        ai_sha1 = ai.get("sha1", "")

        if not ai_url:
            self.log("⚠ Asset index bulunamadı, atlanıyor")
            return

        # Asset index indir
        ai_dest = self.mc_dir / "assets" / "indexes" / f"{ai_id}.json"
        if not (ai_dest.exists() and ai_sha1 and sha1_file(ai_dest) == ai_sha1):
            http_download(ai_url, ai_dest)
            self.log(f"✓ Asset index indirildi: {ai_id}")

        # Objeleri indir
        with open(ai_dest) as f:
            asset_index = json.load(f)

        objects = asset_index.get("objects", {})
        total = len(objects)
        downloaded = 0
        skipped = 0

        for i, (key, obj) in enumerate(objects.items()):
            h = obj.get("hash", "")
            if len(h) < 2:
                continue
            prefix = h[:2]
            dest = self.mc_dir / "assets" / "objects" / prefix / h

            if dest.exists() and sha1_file(dest) == h:
                skipped += 1
            else:
                url = f"https://resources.download.minecraft.net/{prefix}/{h}"
                try:
                    http_download(url, dest)
                    downloaded += 1
                except Exception:
                    pass

            if i % 50 == 0:
                pct = 50 + int((i / max(total, 1)) * 25)
                self.on_progress(pct)
                self.on_status(f"Asset'ler: {i}/{total}")

        self.log(f"✓ Asset'ler: {downloaded} indirildi, {skipped} atlandı")

    def _install_fabric(self):
        self.log("Fabric Loader bilgisi alınıyor...")

        # 1. En son loader versiyonunu al
        loaders = http_get_json(f"{FABRIC_META}/versions/loader/{GAME_VERSION}")
        if not loaders:
            self.log("HATA: Fabric loader bulunamadı!")
            return

        loader_ver = loaders[0]["loader"]["version"]
        self.log(f"Fabric Loader: {loader_ver}")

        # 2. Profil JSON'unu al
        profile_url = f"{FABRIC_META}/versions/loader/{GAME_VERSION}/{loader_ver}/profile/json"
        profile = http_get_json(profile_url)
        ver_id = profile.get("id", f"fabric-loader-{loader_ver}-{GAME_VERSION}")

        # 3. JSON'u kaydet
        ver_dir = self.mc_dir / "versions" / ver_id
        ver_dir.mkdir(parents=True, exist_ok=True)
        with open(ver_dir / f"{ver_id}.json", "w") as f:
            json.dump(profile, f, indent=2)
        self.log(f"✓ Fabric profil JSON kaydedildi: {ver_id}")

        # 4. Fabric kütüphanelerini indir
        libs = profile.get("libraries", [])
        for i, lib in enumerate(libs):
            name = lib.get("name", "")
            path = maven_to_path(name)
            if not path:
                continue

            dest = self.mc_dir / "libraries" / path
            if dest.exists():
                continue

            base_url = lib.get("url", "https://maven.fabricmc.net/")
            if not base_url.endswith("/"):
                base_url += "/"
            url = base_url + path

            try:
                http_download(url, dest)
                self.log(f"  ✓ {name.split(':')[1]}")
            except Exception as e:
                self.log(f"  ⚠ İndirilemedi: {name} — {e}")

            self.on_status(f"Fabric kütüphaneleri: {i + 1}/{len(libs)}")
            self.on_progress(75 + int((i / max(len(libs), 1)) * 15))

        self.log(f"✓ Fabric Loader kuruldu ({len(libs)} kütüphane)")

    def _create_profile(self):
        profiles_path = self.mc_dir / "profiles.json"
        profiles = []

        if profiles_path.exists():
            try:
                with open(profiles_path) as f:
                    profiles = json.load(f)
            except Exception:
                profiles = []

        # Mevcut profili kontrol et
        for p in profiles:
            if p.get("name") == f"Fabric {GAME_VERSION}":
                self.log("✓ Profil zaten mevcut")
                return

        mods_path = str(self.mc_dir / "profiles" / f"Fabric {GAME_VERSION}" / "mods")
        Path(mods_path).mkdir(parents=True, exist_ok=True)

        profiles.append({
            "name": f"Fabric {GAME_VERSION}",
            "gameVersion": GAME_VERSION,
            "loader": "fabric",
            "modsPath": mods_path
        })

        with open(profiles_path, "w") as f:
            json.dump(profiles, f, indent=2)

        self.log(f"✓ Profil oluşturuldu: Fabric {GAME_VERSION}")


# ═══════════════════════════════════════════════════════════════
#  ANA UYGULAMA (Tkinter GUI)
# ═══════════════════════════════════════════════════════════════
class InstallerApp:
    def __init__(self):
        self.root = tk.Tk()
        self.root.title("MixLauncher v2.0 — Kurulum Sihirbazı")
        self.root.geometry("720x580")
        self.root.resizable(False, False)
        self.root.configure(bg=COLORS["bg"])

        # Pencere ortala
        self.root.update_idletasks()
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        x = (sw - 720) // 2
        y = (sh - 580) // 2
        self.root.geometry(f"720x580+{x}+{y}")

        self.installing = False
        self.install_success = False

        self._build_ui()

    def _build_ui(self):
        # ── Başlık Bölgesi ──
        header = tk.Frame(self.root, bg=COLORS["card"], height=100)
        header.pack(fill="x")
        header.pack_propagate(False)

        title = tk.Label(header, text="⛏  MixLauncher v2.0",
                         font=("Segoe UI", 22, "bold"),
                         bg=COLORS["card"], fg=COLORS["white"])
        title.pack(pady=(18, 2))

        subtitle = tk.Label(header, text="Kurulum Sihirbazı — Fabric 1.21.4 Ön Sürüm",
                            font=("Segoe UI", 11),
                            bg=COLORS["card"], fg=COLORS["text_dim"])
        subtitle.pack()

        # ── Ana İçerik ──
        content = tk.Frame(self.root, bg=COLORS["bg"])
        content.pack(fill="both", expand=True, padx=30, pady=15)

        # Kurulum dizini
        dir_frame = tk.Frame(content, bg=COLORS["bg"])
        dir_frame.pack(fill="x", pady=(0, 12))

        tk.Label(dir_frame, text="Kurulum Dizini:",
                 font=("Segoe UI", 10, "bold"),
                 bg=COLORS["bg"], fg=COLORS["text"]).pack(anchor="w")

        dir_box = tk.Frame(dir_frame, bg=COLORS["border"], padx=1, pady=1)
        dir_box.pack(fill="x", pady=(4, 0))
        self.dir_label = tk.Label(dir_box, text=str(MC_DIR),
                                  font=("Consolas", 9),
                                  bg=COLORS["bg_light"], fg=COLORS["warn"],
                                  padx=10, pady=6, anchor="w")
        self.dir_label.pack(fill="x")

        # ── Ön Sürüm Seçeneği ──
        opt_frame = tk.Frame(content, bg=COLORS["bg_light"],
                             highlightbackground=COLORS["border"],
                             highlightthickness=1, padx=15, pady=12)
        opt_frame.pack(fill="x", pady=(0, 12))

        self.prerelease_var = tk.BooleanVar(value=True)
        cb = tk.Checkbutton(opt_frame,
                            text="  Ön Sürümü Etkinleştir (Fabric 1.21.4)",
                            variable=self.prerelease_var,
                            font=("Segoe UI", 11, "bold"),
                            bg=COLORS["bg_light"], fg=COLORS["green"],
                            selectcolor=COLORS["bg"],
                            activebackground=COLORS["bg_light"],
                            activeforeground=COLORS["green"])
        cb.pack(anchor="w")

        desc = tk.Label(opt_frame,
                        text="Minecraft 1.21.4 + Fabric Loader otomatik indirilip kurulacaktır.\n"
                             "Kurulum sonrası internet olmadan oynayabilirsiniz.",
                        font=("Segoe UI", 9),
                        bg=COLORS["bg_light"], fg=COLORS["text_dim"],
                        justify="left")
        desc.pack(anchor="w", pady=(4, 0))

        info = tk.Label(opt_frame,
                        text="📦 İçerik: Vanilla 1.21.4 JAR + Kütüphaneler + Asset'ler + Fabric Loader",
                        font=("Segoe UI", 9),
                        bg=COLORS["bg_light"], fg=COLORS["accent"],
                        justify="left")
        info.pack(anchor="w", pady=(6, 0))

        # ── İlerleme Çubuğu ──
        prog_frame = tk.Frame(content, bg=COLORS["bg"])
        prog_frame.pack(fill="x", pady=(0, 4))

        self.status_label = tk.Label(prog_frame, text="Hazır",
                                     font=("Segoe UI", 10),
                                     bg=COLORS["bg"], fg=COLORS["text_dim"],
                                     anchor="w")
        self.status_label.pack(fill="x")

        style = ttk.Style()
        style.theme_use("default")
        style.configure("Custom.Horizontal.TProgressbar",
                        troughcolor=COLORS["progress_bg"],
                        background=COLORS["progress_fg"],
                        thickness=16)
        self.progress = ttk.Progressbar(prog_frame,
                                        style="Custom.Horizontal.TProgressbar",
                                        maximum=100, value=0)
        self.progress.pack(fill="x", pady=(6, 0))

        self.pct_label = tk.Label(prog_frame, text="",
                                  font=("Segoe UI", 9),
                                  bg=COLORS["bg"], fg=COLORS["text_dim"],
                                  anchor="e")
        self.pct_label.pack(fill="x")

        # ── Log Alanı ──
        log_frame = tk.Frame(content, bg=COLORS["border"], padx=1, pady=1)
        log_frame.pack(fill="both", expand=True, pady=(4, 10))

        self.log_text = tk.Text(log_frame, font=("Consolas", 9),
                                bg=COLORS["bg_light"], fg=COLORS["text"],
                                insertbackground=COLORS["text"],
                                selectbackground=COLORS["accent"],
                                wrap="word", state="disabled",
                                height=8, padx=8, pady=6,
                                borderwidth=0, highlightthickness=0)
        self.log_text.pack(fill="both", expand=True)

        # ── Butonlar ──
        btn_frame = tk.Frame(content, bg=COLORS["bg"])
        btn_frame.pack(fill="x")

        self.install_btn = tk.Button(
            btn_frame, text="🚀  Kurulumu Başlat",
            font=("Segoe UI", 12, "bold"),
            bg=COLORS["green"], fg=COLORS["white"],
            activebackground=COLORS["green_dark"],
            activeforeground=COLORS["white"],
            relief="flat", padx=20, pady=8,
            cursor="hand2",
            command=self._on_install_click)
        self.install_btn.pack(side="left")

        self.launch_btn = tk.Button(
            btn_frame, text="🎮  Launcher'ı Aç",
            font=("Segoe UI", 12, "bold"),
            bg=COLORS["accent"], fg=COLORS["white"],
            activebackground=COLORS["accent_hover"],
            activeforeground=COLORS["white"],
            relief="flat", padx=20, pady=8,
            cursor="hand2", state="disabled",
            command=self._on_launch_click)
        self.launch_btn.pack(side="right")

    # ── Etkileşim Fonksiyonları ──

    def _log(self, msg):
        self.root.after(0, self._append_log, msg)

    def _append_log(self, msg):
        self.log_text.config(state="normal")
        self.log_text.insert("end", msg + "\n")
        self.log_text.see("end")
        self.log_text.config(state="disabled")

    def _set_progress(self, val):
        self.root.after(0, self._update_progress, val)

    def _update_progress(self, val):
        self.progress["value"] = val
        self.pct_label.config(text=f"%{int(val)}")

    def _set_status(self, msg):
        self.root.after(0, self._update_status, msg)

    def _update_status(self, msg):
        self.status_label.config(text=msg)

    def _on_install_click(self):
        if self.installing:
            return

        if not self.prerelease_var.get():
            messagebox.showwarning("Uyarı",
                                   "Lütfen 'Ön Sürümü Etkinleştir' seçeneğini işaretleyin.")
            return

        self.installing = True
        self.install_btn.config(state="disabled", text="⏳  Kuruluyor...",
                                bg=COLORS["text_dim"])

        thread = threading.Thread(target=self._install_thread, daemon=True)
        thread.start()

    def _install_thread(self):
        engine = InstallEngine(
            mc_dir=MC_DIR,
            on_log=self._log,
            on_progress=self._set_progress,
            on_status=self._set_status
        )

        success = engine.run_full_install()
        self.install_success = success

        self.root.after(0, self._install_finished, success)

    def _install_finished(self, success):
        self.installing = False
        if success:
            self.install_btn.config(text="✓  Kurulum Tamamlandı",
                                    bg=COLORS["green_dark"])
            self.launch_btn.config(state="normal")
            self.status_label.config(text="✓ Her şey hazır! Launcher'ı açabilirsiniz.",
                                     fg=COLORS["green"])
        else:
            self.install_btn.config(text="✗  Hata Oluştu — Tekrar Dene",
                                    state="normal", bg=COLORS["accent"])
            self.status_label.config(text="Kurulumda hata oluştu.", fg=COLORS["accent"])

    def _on_launch_click(self):
        exe = LAUNCHER_EXE
        if not exe.exists():
            # Alternatif konumları dene
            alts = [
                SCRIPT_DIR / "build" / "MixLauncher",
                SCRIPT_DIR / "build" / "Release" / "MixLauncher.exe",
                SCRIPT_DIR / "MixLauncher",
                SCRIPT_DIR / "MixLauncher.exe",
            ]
            for alt in alts:
                if alt.exists():
                    exe = alt
                    break
            else:
                messagebox.showerror("Hata",
                                     f"MixLauncher bulunamadı!\n\n"
                                     f"Beklenen konum:\n{LAUNCHER_EXE}\n\n"
                                     "Lütfen önce projeyi derleyin (cmake & make).")
                return

        self._log(f"Launcher başlatılıyor: {exe}")
        try:
            subprocess.Popen([str(exe)], cwd=str(exe.parent))
            self._log("✓ Launcher başlatıldı!")
            self.status_label.config(text="✓ MixLauncher açıldı!", fg=COLORS["green"])

            # 2 saniye sonra kapat
            self.root.after(2000, self.root.destroy)
        except Exception as e:
            messagebox.showerror("Hata", f"Launcher başlatılamadı:\n{e}")

    def run(self):
        self.root.mainloop()


# ═══════════════════════════════════════════════════════════════
if __name__ == "__main__":
    app = InstallerApp()
    app.run()
