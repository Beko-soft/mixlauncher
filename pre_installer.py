import tkinter as tk
from tkinter import ttk, messagebox
import threading
import sys
import os
import zipfile
import shutil
import logging
from pathlib import Path

# ---------- Ayarlar & Sabitler ----------
APP_TITLE = "MinecraftMix Ön Versiyon Kurulumu"
ZIP_FILENAME = "minecraftmix_pack.zip"
TARGET_VERSION_NAME = "Pre-Version"

# Platforma göre hedef dizin (.minecraftmix kökü)
if sys.platform == "win32":
    TARGET_DIR = Path(os.path.expandvars("%APPDATA%")) / ".minecraftmix"
else:
    TARGET_DIR = Path.home() / ".minecraftmix"

# Log dosyası ayarı
LOG_FILE = "minecraftmix_installer.log"
logging.basicConfig(
    filename=LOG_FILE,
    level=logging.INFO,
    format="%(asctime)s [%(levelname)s] %(message)s",
    datefmt="%Y-%m-%d %H:%M:%S"
)

# ---------- Renk Paleti (Dark Theme) ----------
COLORS = {
    "bg": "#121212",             # Ana arka plan, çok koyu gri/siyah
    "card_bg": "#1e1e1e",        # İç çerçeve arka planı
    "text": "#e0e0e0",           # Ana metin rengi
    "text_dim": "#a0a0a0",       # Açıklama metinleri
    "accent": "#bb86fc",         # Vurgu rengi (mor/lila tarzı)
    "accent_hover": "#9965f4",   # Buton hover rengi
    "success": "#03dac6",        # Başarılı (teal)
    "error": "#cf6679",          # Hata rengi (soft kırmızı)
    "progressbar_bg": "#333333", # Progressbar arkaplanı
}

class InstallerApp:
    def __init__(self, root):
        self.root = root
        self.root.title(APP_TITLE)
        self.root.geometry("600x400")
        self.root.configure(bg=COLORS["bg"])
        self.root.resizable(False, False)
        
        # Pencereyi ortala
        self._center_window()
        
        # Style (Ttk için)
        self._setup_styles()
        
        # Değişkenler
        self.install_thread = None
        
        # UI Oluşturma
        self._build_ui()
        logging.info("Installer uygulaması başlatıldı.")

    def _center_window(self):
        self.root.update_idletasks()
        w = self.root.winfo_width()
        h = self.root.winfo_height()
        sw = self.root.winfo_screenwidth()
        sh = self.root.winfo_screenheight()
        x = (sw - w) // 2
        y = (sh - h) // 2
        self.root.geometry(f"{w}x{h}+{x}+{y}")

    def _setup_styles(self):
        style = ttk.Style(self.root)
        style.theme_use("clam")
        
        # Progressbar Stili
        style.configure(
            "TProgressbar",
            troughcolor=COLORS["progressbar_bg"],
            background=COLORS["accent"],
            bordercolor=COLORS["bg"],
            lightcolor=COLORS["accent"],
            darkcolor=COLORS["accent"],
            thickness=8
        )

    def _build_ui(self):
        # Ana Konteyner (Card)
        main_frame = tk.Frame(self.root, bg=COLORS["card_bg"], bd=0)
        main_frame.place(relx=0.05, rely=0.05, relwidth=0.9, relheight=0.9)
        
        # --- Üst Bölüm: Başlık ---
        title_lbl = tk.Label(
            main_frame,
            text=APP_TITLE,
            font=("Segoe UI", 18, "bold"),
            bg=COLORS["card_bg"],
            fg=COLORS["text"]
        )
        title_lbl.pack(pady=(30, 10))
        
        # --- Orta Bölüm: Açıklama ---
        desc_text = (
            "Bu işlem Fabric 1.21.4 ön sürümünü offline olarak kurar.\n"
            "İnternet bağlantısı gerekmez.\n\n"
            f"Hedef: {TARGET_DIR}"
        )
        desc_lbl = tk.Label(
            main_frame,
            text=desc_text,
            font=("Segoe UI", 11),
            bg=COLORS["card_bg"],
            fg=COLORS["text_dim"],
            justify="center"
        )
        desc_lbl.pack(pady=(10, 30))
        
        # --- Alt Bölüm: İşlem ---
        # Buton (Custom Hover özellikli Tkinter butonu)
        self.install_btn = tk.Button(
            main_frame,
            text="Ön Versiyonu Etkinleştir",
            font=("Segoe UI", 12, "bold"),
            bg=COLORS["accent"],
            fg="#000000",
            activebackground=COLORS["accent_hover"],
            activeforeground="#000000",
            relief="flat",
            bd=0,
            cursor="hand2",
            command=self._start_installation
        )
        self.install_btn.pack(pady=(0, 20), ipadx=20, ipady=5)
        
        # Hover Efekti Bindings
        self.install_btn.bind("<Enter>", lambda e: self.install_btn.config(bg=COLORS["accent_hover"]))
        self.install_btn.bind("<Leave>", lambda e: self.install_btn.config(bg=COLORS["accent"]))
        
        # Progressbar
        self.progress = ttk.Progressbar(
            main_frame, 
            orient="horizontal", 
            mode="determinate", 
            style="TProgressbar"
        )
        self.progress.pack(fill="x", padx=40, pady=(0, 15))
        
        # Status Label
        self.status_lbl = tk.Label(
            main_frame,
            text="Hazır.",
            font=("Segoe UI", 10),
            bg=COLORS["card_bg"],
            fg=COLORS["text_dim"]
        )
        self.status_lbl.pack()

    # --- UI Güncelleme Yardımcıları ---
    def _update_status(self, text, color=COLORS["text_dim"]):
        self.status_lbl.config(text=text, fg=color)
        self.root.update_idletasks()
        
    def _update_progress(self, value):
        self.progress["value"] = value
        self.root.update_idletasks()
        
    def _disable_button(self):
        self.install_btn.config(state="disabled", cursor="arrow")
        # Disabled için bind'ları kaldır ki hover çalışmasin
        self.install_btn.unbind("<Enter>")
        self.install_btn.unbind("<Leave>")
        
    def _enable_button(self):
        self.install_btn.config(state="normal", cursor="hand2")
        self.install_btn.bind("<Enter>", lambda e: self.install_btn.config(bg=COLORS["accent_hover"]))
        self.install_btn.bind("<Leave>", lambda e: self.install_btn.config(bg=COLORS["accent"]))

    # --- Kurulum İşlemi ---
    def _start_installation(self):
        self._disable_button()
        self._update_status("Kurulum başlatılıyor...", COLORS["text"])
        self._update_progress(0)
        
        # İşlemi ayrı thread'de başlat
        self.install_thread = threading.Thread(target=self._install_task)
        self.install_thread.daemon = True
        self.install_thread.start()

    def _get_resource_path(self, relative_path):
        """PyInstaller paketinden veya geçerli dizinden dosyayı bulur."""
        try:
            # PyInstaller çalışma dizini
            base_path = sys._MEIPASS
        except Exception:
            base_path = os.path.abspath(".")
        return os.path.join(base_path, relative_path)

    def _install_task(self):
        try:
            logging.info("Kurulum görevi başladı.")
            
            # --- 1. Zip Dosyası Kontrolü ---
            zip_path = self._get_resource_path(ZIP_FILENAME)
            if not os.path.exists(zip_path):
                msg = f"Hata: {ZIP_FILENAME} bulunamadı!"
                logging.error(msg)
                self.root.after(0, self._handle_error, msg)
                return

            self.root.after(0, self._update_status, "Hazırlık yapılıyor...", COLORS["text"])
            self.root.after(0, self._update_progress, 10)

            # --- 2. Zaten Kurulu Mu Kontrolü ---
            pre_version_path = TARGET_DIR / "versions" / TARGET_VERSION_NAME
            if pre_version_path.exists() and pre_version_path.is_dir():
                msg = "Zaten kurulu."
                logging.info(msg)
                self.root.after(0, self._handle_success, msg)
                return

            # --- 3. Çıkarma İşlemi (Extraction) ---
            self.root.after(0, self._update_status, "Dosyalar çıkartılıyor, lütfen bekleyin...", COLORS["text"])
            
            # Geçici bir klasöre çıkar (Yarım kalmamasi için atomik işlem yaklaşimi)
            temp_extract_dir = TARGET_DIR / ".temp_extract"
            
            # Eğer temp varsa temizle
            if temp_extract_dir.exists():
                shutil.rmtree(temp_extract_dir)
            temp_extract_dir.mkdir(parents=True, exist_ok=True)

            try:
                with zipfile.ZipFile(zip_path, 'r') as zip_ref:
                    # Gelişmiş progress için
                    file_list = zip_ref.namelist()
                    total_files = len(file_list)
                    
                    for i, member in enumerate(file_list):
                        zip_ref.extract(member, path=temp_extract_dir)
                        # %10'dan başlayip %90'a kadar progress
                        progress_val = 10 + int((i / total_files) * 80)
                        if i % max(1, total_files // 100) == 0:  # UI'i boğmamak için %1'de bir güncelle
                            self.root.after(0, self._update_progress, progress_val)
                            
                logging.info("Zip dosyası geçici klasöre başarıyla çıkartıldı.")
                
            except zipfile.BadZipFile:
                msg = "Hata: Zip dosyası bozuk!"
                logging.error(msg)
                if temp_extract_dir.exists():
                    shutil.rmtree(temp_extract_dir)
                self.root.after(0, self._handle_error, msg)
                return
            except PermissionError:
                msg = "Hata: Yazma izni yok!"
                logging.error(msg)
                if temp_extract_dir.exists():
                    shutil.rmtree(temp_extract_dir)
                self.root.after(0, self._handle_error, msg)
                return
            except OSError as e:
                # Disk dolu vs.
                msg = f"Hata: G/Ç Hatası - {e.strerror}"
                logging.error(msg)
                if temp_extract_dir.exists():
                    shutil.rmtree(temp_extract_dir)
                self.root.after(0, self._handle_error, msg)
                return

            self.root.after(0, self._update_status, "Dosyalar yerleştiriliyor...", COLORS["text"])
            self.root.after(0, self._update_progress, 90)

            # --- 4. Dosyaları Taşıma (Merge without overwrite of existing modified files if needed, but per request: Tümüyle extract) ---
            # Mevcut dizinlerle birleştirme işlemi. shutil.copytree(dirs_exist_ok=True) Python 3.8+ gerektirir.
            try:
                for item in temp_extract_dir.iterdir():
                    dest = TARGET_DIR / item.name
                    if item.is_dir():
                        shutil.copytree(item, dest, dirs_exist_ok=True)
                    else:
                        shutil.copy2(item, dest)
                
                # Temp'i temizle
                shutil.rmtree(temp_extract_dir)
                logging.info("Dosyalar hedef klasöre başarıyla yerleştirildi.")

            except Exception as e:
                msg = f"Taşıma Hatası: {str(e)}"
                logging.error(msg)
                self.root.after(0, self._handle_error, msg)
                return

            # --- 5. Başarılı Bitiş ---
            self.root.after(0, self._update_progress, 100)
            self.root.after(0, self._handle_success, "Kurulum tamamlandı.")

        except Exception as e:
            msg = f"Bilinmeyen Hata: {str(e)}"
            logging.error(msg, exc_info=True)
            self.root.after(0, self._handle_error, "Kritik bir hata oluştu. Loka bakın.")
            
    def _handle_error(self, message):
        self._update_status(message, COLORS["error"])
        # Hatadan sonra butonu geri aktif edebiliriz (kullanıcı belki zip'i ayni yere koyup tekrar dener)
        self._enable_button()
        messagebox.showerror("Kurulum Hatası", message)
        
    def _handle_success(self, message):
        self._update_status(message, COLORS["success"])
        # Buton pasif kalir
        messagebox.showinfo("Bilgi", message)

if __name__ == "__main__":
    root = tk.Tk()
    app = InstallerApp(root)
    root.mainloop()
