#pragma once

#include <QComboBox>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QSlider>
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>
#include <QVector>
#include <memory>

#include "AnimatedTabBar.h"
#include "AuthManager.h"
#include "LauncherCore.h"
#include "ModManager.h"

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  explicit MainWindow(QWidget *parent = nullptr);
  ~MainWindow() override;

protected:
  void paintEvent(QPaintEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private slots:
  // UI Yapılandırma
  void buildUi();
  void applyGlobalStyle();

  // Core Sinyalleri
  void onVersionsReady(QStringList ids);
  void onInstallProgress(int done, int total, QString currentFile);
  void onInstallDone(bool ok, QString errorMsg);
  void onLaunchClicked();

  // Mağaza / Modlar
  void onSearchMod();
  void onModSearchResults(QVector<ModSearchResult> results);
  void onInstallSelectedMod();
  void onModInstalled(QString modName, bool success);

  // Auth / Hesap
  void onLoginElyBy();
  void onLoginOffline();
  void onLoginResult(AuthSession session);
  void onLoginError(QString error);
  void onSkinReady(QPixmap skin);
  void onUploadSkin();

  // Ayarlar
  void onChangeBgColor();
  void onInstallVersion(); // Popup

private:
  std::unique_ptr<LauncherCore> m_core;

  // UI Bileşenleri
  QStackedWidget *m_stack;
  AnimatedTabBar *m_tabBar;

  // Alt Bar
  QLabel *m_statusLabel;
  QProgressBar *m_progress;

  // Oyun Sayfası
  QComboBox *m_verCombo; // Sürüm Seçimi
  QPushButton *m_launchBtn;

  // Mağaza Sayfası
  QComboBox *m_storeVerCombo;  // Hedef Sürüm
  QComboBox *m_storeTypeCombo; // Mod/Texture/World
  QLineEdit *m_modSearch;
  QListWidget *m_modList;

  // Kostüm Sayfası
  QLabel *m_skinFullPreview;

  // Ayarlar Sayfası
  QLineEdit *m_elyUser;
  QLineEdit *m_elyPass;
  QLineEdit *m_offlineUser;
  QLabel *m_authStatus;
  QSlider *m_ramSlider;
  QLabel *m_ramLabel;
  QComboBox *m_profileCombo;

  // Görünüm
  QColor m_bgColor = QColor(20, 20, 25);
  QPixmap m_bgPixmap;

  // Sayfa Oluşturucular
  QWidget *buildGamePage();
  QWidget *buildModStorePage();
  QWidget *buildCostumePage();
  QWidget *buildSettingsPage();
};
