#include "MainWindow.h"
#include "AnimatedTabBar.h"
#include "AuthManager.h"
#include "DownloadManager.h"
#include "LauncherCore.h"
#include "ModManager.h"

// Qt Includes
#include <QApplication>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGraphicsDropShadowEffect>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTimer>
#include <QVBoxLayout>

#include <spdlog/spdlog.h>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
  setWindowTitle(QString::fromUtf8("MixLauncher v2.0"));
  setAcceptDrops(true);
  resize(960, 640);

  m_core = std::make_unique<LauncherCore>();

  buildUi();
  applyGlobalStyle();

  // Sinyal Bağlantıları
  connect(m_core.get(), &LauncherCore::versionsReady, this,
          &MainWindow::onVersionsReady);
  connect(m_core.get(), &LauncherCore::installProgress, this,
          &MainWindow::onInstallProgress);
  connect(m_core.get(), &LauncherCore::installFinished, this,
          &MainWindow::onInstallDone);

  // Mod Manager
  connect(m_core->mods(), &ModManager::searchFinished, this,
          &MainWindow::onModSearchResults);
  connect(m_core->mods(), &ModManager::modInstalled, this,
          &MainWindow::onModInstalled);
  connect(m_core->mods(), &ModManager::profilesChanged, this, [this]() {
    m_verCombo->clear();
    m_storeVerCombo->clear();
    m_profileCombo->clear();
    auto profiles = m_core->mods()->listProfiles();
    for (const auto &p : profiles) {
      m_verCombo->addItem(p.name, p.gameVersion);
      m_storeVerCombo->addItem(p.name, p.gameVersion);
      m_profileCombo->addItem(p.name);
    }
  });

  // Auth Manager
  connect(m_core->auth(), &AuthManager::loginSuccess, this,
          &MainWindow::onLoginResult);
  connect(m_core->auth(), &AuthManager::loginFailed, this,
          &MainWindow::onLoginError);
  connect(m_core->auth(), &AuthManager::skinReady, this,
          &MainWindow::onSkinReady);

  // Başlangıç
  QTimer::singleShot(100, [this]() { m_core->fetchVersionManifest(); });
}

MainWindow::~MainWindow() = default;

void MainWindow::paintEvent(QPaintEvent *) {
  QPainter p(this);
  p.setRenderHint(QPainter::Antialiasing);

  if (!m_bgPixmap.isNull()) {
    p.drawPixmap(rect(),
                 m_bgPixmap.scaled(size(), Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation));
    p.fillRect(rect(), QColor(0, 0, 0, 140)); // Karartma
  } else {
    p.fillRect(rect(), m_bgColor);

    // Soyut Arkaplan (Siyah/Gri Küreler)
    QRadialGradient g1(width() * 0.2, height() * 0.6, height() * 0.5);
    g1.setColorAt(0, QColor(40, 40, 40, 100)); // Dark Gray
    g1.setColorAt(1, Qt::transparent);
    p.setBrush(g1);
    p.setPen(Qt::NoPen);
    p.drawEllipse(g1.center(), height() * 0.5, height() * 0.5);

    QRadialGradient g2(width() * 0.8, height() * 0.4, height() * 0.6);
    g2.setColorAt(0, QColor(60, 60, 60, 50)); // Lighter Gray
    g2.setColorAt(1, Qt::transparent);
    p.setBrush(g2);
    p.drawEllipse(g2.center(), height() * 0.6, height() * 0.6);
  }
}

void MainWindow::buildUi() {
  auto *c = new QWidget(this);
  setCentralWidget(c);
  auto *root = new QVBoxLayout(c);
  root->setContentsMargins(0, 0, 0, 0);
  root->setSpacing(0);

  m_stack = new QStackedWidget;
  m_stack->addWidget(buildGamePage());
  m_stack->addWidget(buildModStorePage()); // İsim Değişti -> "Mağaza"
  m_stack->addWidget(buildCostumePage());
  m_stack->addWidget(buildSettingsPage());

  root->addWidget(m_stack, 1);

  // Alt Bar (Durum + İlerleme)
  auto *botBar = new QFrame;
  botBar->setFixedHeight(32);
  botBar->setStyleSheet(
      "background:rgba(20, 20, 20, 0.9); border-top:1px solid #333;");
  auto *bl = new QHBoxLayout(botBar);
  bl->setContentsMargins(10, 0, 10, 0);

  m_statusLabel = new QLabel("Hazır");
  m_statusLabel->setStyleSheet("color:#AAA; font-size:11px;");
  bl->addWidget(m_statusLabel);

  m_progress = new QProgressBar;
  m_progress->setFixedWidth(200);
  m_progress->setFixedHeight(12);
  m_progress->setTextVisible(false);
  bl->addWidget(m_progress);

  root->addWidget(botBar);

  // Tab Bar
  m_tabBar = new AnimatedTabBar;
  m_tabBar->addTab("Oyun");
  m_tabBar->addTab("Mağaza");
  m_tabBar->addTab("Kostüm");
  m_tabBar->addTab("Ayarlar");
  connect(m_tabBar, &AnimatedTabBar::currentChanged, this,
          [this](int i) { m_stack->setCurrentIndex(i); });

  root->addWidget(m_tabBar);
}

// ════════════════════════OYUN SAYFASI════════════════════════
QWidget *MainWindow::buildGamePage() {
  auto *p = new QWidget;
  auto *v = new QVBoxLayout(p);
  v->setContentsMargins(40, 40, 40, 40);

  // Üst: Sürüm Seçimi
  auto *top = new QHBoxLayout;
  top->addStretch();

  m_verCombo = new QComboBox;
  m_verCombo->setMinimumWidth(240);
  m_verCombo->setFixedHeight(36);
  m_verCombo->setPlaceholderText("Sürüm Seçiniz");
  top->addWidget(m_verCombo);

  auto *btnAdd = new QPushButton("+");
  btnAdd->setFixedHeight(36);
  btnAdd->setStyleSheet(
      "font-size:20px; font-weight:bold; border-radius:6px; "
      "background-color:rgba(255,255,255,0.15); padding: 0 15px;");
  connect(btnAdd, &QPushButton::clicked, this, &MainWindow::onInstallVersion);
  top->addWidget(btnAdd);

  top->addStretch();
  v->addLayout(top);

  v->addStretch(2);

  // Orta: Logo
  auto *title = new QLabel("MİXLAUNCHER");
  title->setAlignment(Qt::AlignCenter);
  title->setStyleSheet(
      "font-size:48px; font-weight:900; color:white; letter-spacing:4px;");
  auto *shadow = new QGraphicsDropShadowEffect;
  shadow->setBlurRadius(40);
  shadow->setColor(QColor(0, 0, 0, 200));
  shadow->setOffset(0, 5);
  title->setGraphicsEffect(shadow);
  v->addWidget(title);

  v->addStretch(2);

  // Alt: OYNA Butonu
  m_launchBtn = new QPushButton("Oyna");
  m_launchBtn->setFixedHeight(60);
  m_launchBtn->setCursor(Qt::PointingHandCursor);
  m_launchBtn->setStyleSheet(
      "background-color:rgba(255,255,255,0.15); font-size:24px; "
      "font-weight:500; letter-spacing:1px; border-radius:6px; border:1px "
      "solid rgba(255,255,255,0.05);");
  connect(m_launchBtn, &QPushButton::clicked, this,
          &MainWindow::onLaunchClicked);

  v->addWidget(m_launchBtn);
  v->addSpacing(20);

  return p;
}

// ════════════════════════MAĞAZA (Mod/Doku/Dünya)════════════════════════
QWidget *MainWindow::buildModStorePage() {
  auto *p = new QWidget;
  auto *v = new QVBoxLayout(p);
  v->setContentsMargins(20, 20, 20, 20);
  v->setSpacing(10);

  auto *header = new QLabel("İçerik Mağazası");
  header->setStyleSheet("font-size:22px; font-weight:bold; color:white;");
  v->addWidget(header);

  // Filtreler
  auto *h = new QHBoxLayout;

  // 1. Hangi Sürüm İçin?
  auto *lblVer = new QLabel("Hedef:");
  h->addWidget(lblVer);

  m_storeVerCombo = new QComboBox;
  m_storeVerCombo->addItem("Seçili Profil Yok");
  h->addWidget(m_storeVerCombo);

  // 2. Ne İndirilecek?
  auto *lblType = new QLabel("Tür:");
  lblType->setStyleSheet("margin-left:10px;");
  h->addWidget(lblType);

  m_storeTypeCombo = new QComboBox;
  m_storeTypeCombo->addItems({"Mod", "Doku Paketi", "Mod Paketi"});
  h->addWidget(m_storeTypeCombo);

  h->addStretch();
  v->addLayout(h);

  // Arama
  auto *searchBox = new QHBoxLayout;
  m_modSearch = new QLineEdit;
  m_modSearch->setPlaceholderText("İçerik Ara... (Sodium, Faithful...)");
  connect(m_modSearch, &QLineEdit::returnPressed, this,
          &MainWindow::onSearchMod);
  searchBox->addWidget(m_modSearch);

  auto *btnSearch = new QPushButton("Ara");
  btnSearch->setCursor(Qt::PointingHandCursor);
  connect(btnSearch, &QPushButton::clicked, this, &MainWindow::onSearchMod);
  searchBox->addWidget(btnSearch);

  v->addLayout(searchBox);

  // Liste
  m_modList = new QListWidget;
  v->addWidget(m_modList);

  // İndir Butonu
  auto *btnInstall = new QPushButton("Seçileni İndir");
  btnInstall->setCursor(Qt::PointingHandCursor);
  btnInstall->setStyleSheet("background-color: rgba(255,255,255,0.15);");
  connect(btnInstall, &QPushButton::clicked, this,
          &MainWindow::onInstallSelectedMod);
  v->addWidget(btnInstall);

  return p;
}

// ════════════════════════KOSTÜM════════════════════════
QWidget *MainWindow::buildCostumePage() {
  auto *p = new QWidget;
  auto *v = new QVBoxLayout(p);
  v->setContentsMargins(40, 40, 40, 40);

  auto *card = new QFrame;
  auto *cv = new QVBoxLayout(card);
  cv->setContentsMargins(20, 20, 20, 20);

  auto *title = new QLabel("Kostüm Yönetimi");
  title->setStyleSheet("font-size:18px; font-weight:bold; color:white;");
  title->setAlignment(Qt::AlignCenter);
  cv->addWidget(title);

  m_skinFullPreview = new QLabel("Skin\nÖnizleme");
  m_skinFullPreview->setFixedSize(128, 128);
  m_skinFullPreview->setAlignment(Qt::AlignCenter);
  m_skinFullPreview->setStyleSheet("background:#222; border:2px dashed #555; "
                                   "border-radius:8px; color:#aaa;");

  auto *centerLayout = new QHBoxLayout;
  centerLayout->addStretch();
  centerLayout->addWidget(m_skinFullPreview);
  centerLayout->addStretch();
  cv->addLayout(centerLayout);

  auto *btnUp = new QPushButton("Dosyadan Yükle (.png)");
  btnUp->setCursor(Qt::PointingHandCursor);
  connect(btnUp, &QPushButton::clicked, this, &MainWindow::onUploadSkin);
  cv->addWidget(btnUp);

  v->addWidget(card);
  v->addStretch();
  return p;
}

// ════════════════════════AYARLAR════════════════════════
QWidget *MainWindow::buildSettingsPage() {
  auto *s = new QScrollArea;
  s->setWidgetResizable(true);
  s->setStyleSheet("background:transparent; border:none;");
  auto *p = new QWidget;
  auto *v = new QVBoxLayout(p);
  v->setContentsMargins(40, 20, 40, 40);
  v->setSpacing(25);

  QString borderedBtnStyle =
      "QPushButton { border: 1px solid rgba(255, 255, 255, 0.3); "
      "border-radius: 4px; padding: 6px 12px; } QPushButton:hover { "
      "background: rgba(255,255,255,0.2); }";

  // 1. Hesap
  auto *gAuth = new QGroupBox("Hesap (Ely.by / Offline)");
  auto *fAuth = new QFormLayout(gAuth);
  fAuth->setSpacing(15);
  fAuth->setContentsMargins(20, 20, 20, 20);

  m_elyUser = new QLineEdit;
  m_elyUser->setPlaceholderText("Kullanıcı Adı / E-posta");
  m_elyPass = new QLineEdit;
  m_elyPass->setEchoMode(QLineEdit::Password);
  m_offlineUser = new QLineEdit;
  m_offlineUser->setPlaceholderText("Steve");

  auto *btnLogin = new QPushButton("Ely.by Giriş");
  btnLogin->setCursor(Qt::PointingHandCursor);
  btnLogin->setStyleSheet(borderedBtnStyle);
  connect(btnLogin, &QPushButton::clicked, this, &MainWindow::onLoginElyBy);

  auto *btnOffline = new QPushButton("Offline Giriş");
  btnOffline->setCursor(Qt::PointingHandCursor);
  btnOffline->setStyleSheet(borderedBtnStyle);
  connect(btnOffline, &QPushButton::clicked, this, &MainWindow::onLoginOffline);

  fAuth->addRow("Ely.by Kullanıcı:", m_elyUser);
  fAuth->addRow("Ely.by Şifre:", m_elyPass);
  fAuth->addRow("", btnLogin);
  fAuth->addRow("Offline Ad:", m_offlineUser);
  fAuth->addRow("", btnOffline);

  m_authStatus = new QLabel("Giriş Yapılmadı");
  m_authStatus->setStyleSheet("color:#aaa; font-style:italic;");
  fAuth->addRow("Durum:", m_authStatus);

  v->addWidget(gAuth);

  // 2. RAM
  auto *gRam = new QGroupBox("Bellek (RAM)");
  auto *hRam = new QHBoxLayout(gRam);
  hRam->setContentsMargins(20, 20, 20, 20);
  m_ramSlider = new QSlider(Qt::Horizontal);
  m_ramSlider->setRange(1024, 16384);
  m_ramSlider->setValue(4096);
  m_ramLabel = new QLabel("4.0 GB");
  connect(m_ramSlider, &QSlider::valueChanged, [this](int val) {
    m_ramLabel->setText(QString::number(val / 1024.0, 'f', 1) + " GB");
  });
  hRam->addWidget(m_ramSlider);
  hRam->addWidget(m_ramLabel);
  v->addWidget(gRam);

  // 3. Arkaplan
  auto *gBg = new QGroupBox("Görünüm");
  auto *hBg = new QHBoxLayout(gBg);
  hBg->setContentsMargins(20, 20, 20, 20);
  auto *btnBgImg = new QPushButton("Resim Seç");
  btnBgImg->setStyleSheet(borderedBtnStyle);
  connect(btnBgImg, &QPushButton::clicked, this, [this]() {
    QString f = QFileDialog::getOpenFileName(this, "Resim Seç", "",
                                             "Images (*.png *.jpg)");
    if (!f.isEmpty()) {
      m_bgPixmap.load(f);
      update();
    }
  });
  auto *btnBgColor = new QPushButton("Renk Seç");
  btnBgColor->setStyleSheet(borderedBtnStyle);
  connect(btnBgColor, &QPushButton::clicked, this,
          &MainWindow::onChangeBgColor);
  hBg->addWidget(btnBgImg);
  hBg->addWidget(btnBgColor);
  v->addWidget(gBg);

  // 4. Profil Yönetimi
  auto *gProf = new QGroupBox("Profil Yönetimi");
  auto *hProf = new QHBoxLayout(gProf);
  hProf->setContentsMargins(20, 20, 20, 20);
  m_profileCombo = new QComboBox;
  m_profileCombo->setStyleSheet("border: 1px solid rgba(255, 255, 255, 0.3); "
                                "border-radius: 4px; padding: 6px 12px;");
  auto *btnDel = new QPushButton("Profili Sil");
  btnDel->setStyleSheet(borderedBtnStyle);
  connect(btnDel, &QPushButton::clicked, [this]() {
    if (m_profileCombo->count() > 0) {
      QString name = m_profileCombo->currentText();
      m_core->mods()->deleteProfile(name);
    }
  });
  hProf->addWidget(m_profileCombo);
  hProf->addWidget(btnDel);
  v->addWidget(gProf);

  v->addStretch();
  s->setWidget(p);
  return s;
}

// ════════ Lojik Fonksiyonlar ════════
void MainWindow::onVersionsReady(QStringList /*ids*/) {
  m_verCombo->clear();
  m_storeVerCombo->clear();
  m_profileCombo->clear();

  // Yalnızca profilleri yükle
  auto profiles = m_core->mods()->listProfiles();
  for (const auto &p : profiles) {
    m_verCombo->addItem(p.name, p.gameVersion);
    m_storeVerCombo->addItem(p.name, p.gameVersion);
    m_profileCombo->addItem(p.name);
  }

  if (m_verCombo->count() == 0) {
    m_verCombo->setPlaceholderText("Sürüm Seçiniz");
  }

  if (m_storeVerCombo->count() == 0) {
    m_storeVerCombo->setPlaceholderText("Seçili Profil Yok");
  }

  m_statusLabel->setText("Sürümler Hazır.");
}

void MainWindow::onInstallVersion() {
  QDialog d(this);
  d.setWindowTitle("Yeni Sürüm Oluştur");
  d.setFixedSize(400, 300);
  d.setStyleSheet("background:#222; color:white;");

  QVBoxLayout l(&d);

  QLabel lbl("Sürüm Adı:");
  l.addWidget(&lbl);
  QLineEdit logName;
  logName.setStyleSheet(
      "background:#333; border:1px solid #555; padding:5px; color:white;");
  l.addWidget(&logName);

  QLabel lblVer("Minecraft Sürümü:");
  l.addWidget(&lblVer);
  QComboBox cbVer;
  // Cache'den doldur
  for (const auto &v : m_core->cachedVersions())
    if (v.type == "release")
      cbVer.addItem(QString::fromStdString(v.id));
  l.addWidget(&cbVer);

  QLabel lblLoader("Mod Yükleyici:");
  l.addWidget(&lblLoader);
  QComboBox cbLoader;
  cbLoader.addItems({"Yok (Vanilla)", "Fabric", "Forge", "Quilt"});
  l.addWidget(&cbLoader);

  QPushButton btn("OLUŞTUR");
  btn.setStyleSheet("background:#28a745; border:none; padding:10px; "
                    "font-weight:bold; margin-top:10px;");
  l.addWidget(&btn);

  connect(&btn, &QPushButton::clicked, [&]() { d.accept(); });

  if (d.exec() == QDialog::Accepted) {
    QString name = logName.text().trimmed();
    QString ver = cbVer.currentText();
    QString loader = cbLoader.currentText();

    if (name.isEmpty())
      name = ver;

    // İndirme Başlat
    m_statusLabel->setText(
        QString("Sürüm İndiriliyor: %1 (%2)").arg(name, ver));

    // 1. Vanilla İndir
    m_core->installVersion(ver);

    // 2. Modloader (Sonraki aşamada, install bitince tetiklenmesi daha doğru
    // ama şimdilik sync varsayalım ya da chain)
    if (loader.contains("Fabric"))
      m_core->mods()->installFabric(ver);
    else if (loader.contains("Quilt"))
      m_core->mods()->installQuilt(ver);
    else if (loader.contains("Forge"))
      m_core->mods()->installForge(ver);

    // 3. Profil Oluştur
    m_core->mods()->createProfile(name, ver, loader.toLower());

    // Profil store ve settings için güncellenecek
    // Profil listesini onVersionsReady ya da signal ile tazeliyor olacağız.
    // Ancak manuel olarak da güncelleyebiliriz:
    m_verCombo->addItem(name, ver);
    // m_store/profileCombo will be updated via ModManager::profilesChanged

    m_verCombo->setCurrentText(name);
  }
}

void MainWindow::onInstallProgress(int d, int t, QString f) {
  if (t > 0) {
    int pct = (d * 100) / t;
    m_progress->setValue(pct);

    // Buton Progress
    double stop = pct / 100.0;
    QString style =
        QString("QPushButton{background:qlineargradient(x1:0, y1:0, x2:1, "
                "y2:0, stop:0 rgba(255,255,255,0.4), stop:%1 "
                "rgba(255,255,255,0.4), stop:%2 "
                "rgba(255,255,255,0.1), stop:1 rgba(255,255,255,0.1));"
                "color:white; font-size:20px; font-weight:bold; "
                "border-radius:6px;}")
            .arg(stop, 0, 'f', 2)
            .arg(stop + 0.001, 0, 'f', 2);

    m_launchBtn->setStyleSheet(style);
    m_launchBtn->setText(QString("YÜKLENİYOR %1%").arg(pct));
  }
  m_statusLabel->setText(QString("İndiriliyor: %1").arg(f));
}

void MainWindow::onInstallDone(bool ok, QString msg) {
  m_progress->setValue(ok ? 100 : 0);
  m_statusLabel->setText(ok ? "İşlem Tamamlandı." : "Hata: " + msg);

  // Butonu Düzelt
  m_launchBtn->setText("Oyna");
  m_launchBtn->setStyleSheet(
      "background-color:rgba(255,255,255,0.15); font-size:24px; "
      "font-weight:500; letter-spacing:1px; border-radius:6px; border:1px "
      "solid rgba(255,255,255,0.05);");
}

void MainWindow::onLaunchClicked() {
  QString ver = m_verCombo->currentData().toString();
  if (ver.isEmpty()) {
    QMessageBox::warning(this, "Seçim", "Önce bir sürüm seçin veya oluşturun.");
    return;
  }

  int ram = m_ramSlider->value();
  QString prof = m_verCombo->currentText(); // Profil adı combo textidir

  m_statusLabel->setText("Oyun Başlatılıyor...");
  m_launchBtn->setEnabled(false);

  m_core->launchGame(ver, ram, prof);

  // 5 saniye sonra butonu aç
  QTimer::singleShot(5000, [this]() { m_launchBtn->setEnabled(true); });
}

void MainWindow::onSearchMod() {
  QString q = m_modSearch->text();
  QString type = "mod"; // Varsayılan

  // Combo'dan seçilen tip
  QString comboType = m_storeTypeCombo->currentText();
  if (comboType == "Doku Paketi")
    type = "resourcepack";
  else if (comboType == "Mod Paketi")
    type = "modpack";

  QString targetProfile = m_storeVerCombo->currentText();
  if (targetProfile.isEmpty() || targetProfile == "Seçili Profil Yok") {
    QMessageBox::warning(this, "Uyarı",
                         "Arama yapmak için önce bir profil oluşturun.");
    return;
  }

  QString ver;
  QString loader;
  auto profiles = m_core->mods()->listProfiles();
  for (const auto &p : profiles) {
    if (p.name == targetProfile) {
      ver = p.gameVersion;
      loader = p.loader;
      break;
    }
  }

  if (ver.isEmpty())
    return;
  if (loader.toLower().contains("yok") || loader.toLower() == "vanilla") {
    loader = "vanilla";
  }

  m_statusLabel->setText("Aranıyor...");
  m_core->mods()->searchMods(q, loader, ver, type);
}

void MainWindow::onModSearchResults(QVector<ModSearchResult> res) {
  m_modList->clear();
  for (auto &r : res) {
    auto *item = new QListWidgetItem(QString("%1\nYazar: %2 | İndirme: %3")
                                         .arg(r.title, r.author)
                                         .arg(r.downloads));
    item->setData(Qt::UserRole, r.projectId);
    m_modList->addItem(item);
  }
  m_statusLabel->setText(QString("%1 sonuç bulundu.").arg(res.size()));
}

void MainWindow::onInstallSelectedMod() {
  auto l = m_modList->selectedItems();
  if (l.isEmpty())
    return;

  QString pid = l[0]->data(Qt::UserRole).toString();
  QString targetProfile = m_storeVerCombo->currentText();

  if (targetProfile.isEmpty() || targetProfile == "Seçili Profil Yok") {
    QMessageBox::warning(this, "Hata", "Hedef profil seçili değil.");
    return;
  }

  QString ver;
  QString loader;
  auto profiles = m_core->mods()->listProfiles();
  for (const auto &p : profiles) {
    if (p.name == targetProfile) {
      ver = p.gameVersion;
      loader = p.loader;
      break;
    }
  }

  if (loader.toLower().contains("yok") || loader.toLower() == "vanilla") {
    loader = "vanilla";
  }

  m_statusLabel->setText("İçerik İndiriliyor...");
  m_core->mods()->installMod(pid, loader, ver, targetProfile);
}

void MainWindow::onModInstalled(QString name, bool ok) {
  if (ok)
    QMessageBox::information(this, "Başarılı", name + " kuruldu!");
  else
    QMessageBox::critical(this, "Hata", name + " kurulamadı!");
}

// Diğer slotlar (Skin, Login, BgColor) implementation as previously logic...
void MainWindow::onLoginElyBy() {
  m_core->auth()->loginElyBy(m_elyUser->text(), m_elyPass->text());
}
void MainWindow::onLoginOffline() {
  m_core->auth()->loginOffline(m_offlineUser->text());
}
void MainWindow::onLoginResult(AuthSession s) {
  m_authStatus->setText("Giriş: " + s.username + " (" + s.authType + ")");
  if (s.authType == "elyby")
    m_core->auth()->fetchSkin(s.username);
}
void MainWindow::onLoginError(QString e) {
  QMessageBox::critical(this, "Login Hatası", e);
}
void MainWindow::onSkinReady(QPixmap p) {
  m_skinFullPreview->setPixmap(p.scaled(128, 128, Qt::KeepAspectRatio));
}
void MainWindow::onChangeBgColor() {
  QColor c = QColorDialog::getColor(Qt::black, this);
  if (c.isValid()) {
    m_bgColor = c;
    m_bgPixmap = QPixmap();
    update();
  }
}
void MainWindow::onUploadSkin() {
  QString f = QFileDialog::getOpenFileName(this, "Skin Seç", "", "PNG (*.png)");
  if (!f.isEmpty()) {
    m_skinFullPreview->setPixmap(
        QPixmap(f).scaled(128, 128, Qt::KeepAspectRatio));
  }
}
void MainWindow::dragEnterEvent(QDragEnterEvent *e) {
  if (e->mimeData()->hasUrls())
    e->acceptProposedAction();
}
void MainWindow::dropEvent(QDropEvent *e) { /* Drag drop logic */ }
void MainWindow::applyGlobalStyle() {
  m_bgColor = QColor(24, 24, 24); // dark gray
  setStyleSheet(R"CSS(
    * {
      font-family: 'Segoe UI', 'Inter', sans-serif;
      color: #E0E0E0;
    }
    QMainWindow, QDialog {
      background-color: #181818;
    }
    QWidget {
      background: transparent;
    }
    QPushButton {
      background-color: rgba(255, 255, 255, 0.1);
      border: 1px solid rgba(255, 255, 255, 0.05);
      border-radius: 4px;
      color: #FFFFFF;
      padding: 8px 16px;
      font-weight: 500;
      font-size: 13px;
    }
    QPushButton:hover {
      background-color: rgba(255, 255, 255, 0.2);
    }
    QPushButton:pressed {
      background-color: rgba(255, 255, 255, 0.05);
    }
    QPushButton:disabled {
      background-color: rgba(255, 255, 255, 0.02);
      color: #777777;
    }
    QComboBox, QLineEdit {
      background-color: rgba(255, 255, 255, 0.08);
      border: 1px solid rgba(255, 255, 255, 0.1);
      padding: 8px 12px;
      border-radius: 4px;
      color: #FFFFFF;
    }
    QComboBox:hover, QLineEdit:focus {
      border: 1px solid #777777;
    }
    QComboBox::drop-down {
      border: none;
      width: 30px;
    }
    QComboBox::down-arrow {
      image: none;
      border-left: 5px solid transparent;
      border-right: 5px solid transparent;
      border-top: 5px solid #AAAAAA;
      width: 0;
      height: 0;
      margin-right: 15px;
    }
    QComboBox QAbstractItemView {
      background-color: #222222;
      border: 1px solid #444444;
      selection-background-color: rgba(255, 255, 255, 0.1);
      outline: none;
    }
    QGroupBox {
      border: 1px solid rgba(255, 255, 255, 0.1);
      border-radius: 6px;
      margin-top: 1.2em;
      background-color: rgba(255, 255, 255, 0.02);
    }
    QGroupBox::title {
      subcontrol-origin: margin;
      left: 10px;
      padding: 0 5px;
      color: #AAAAAA;
      font-weight: bold;
    }
    QListWidget {
      background-color: rgba(255, 255, 255, 0.02);
      border: 1px solid rgba(255, 255, 255, 0.1);
      border-radius: 4px;
    }
    QListWidget::item {
      padding: 10px;
      border-bottom: 1px solid rgba(255, 255, 255, 0.05);
    }
    QListWidget::item:selected {
      background-color: rgba(255, 255, 255, 0.1);
      border-left: 3px solid #AAAAAA;
      color: #FFFFFF;
      font-weight: bold;
    }
    QScrollBar:vertical {
      border: none;
      background: transparent;
      width: 10px;
      margin: 0px 0px 0px 0px;
    }
    QScrollBar::handle:vertical {
      background: rgba(255, 255, 255, 0.2);
      border-radius: 5px;
      min-height: 20px;
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
      height: 0px;
    }
    QProgressBar {
      background-color: rgba(255, 255, 255, 0.1);
      border: none;
      border-radius: 4px;
      text-align: center;
      color: transparent;
    }
    QProgressBar::chunk {
      background-color: #888888;
      border-radius: 4px;
    }
    QToolTip {
      background: #222222;
      color: white;
      border: 1px solid #444444;
    }
  )CSS");
}
