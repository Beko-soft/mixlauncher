#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

struct VersionEntry {
  std::string id;
  std::string type; // "release" | "snapshot"
  std::string url;  // version-JSON url
};

class DownloadManager;
class ModManager;
class AuthManager;

class LauncherCore : public QObject {
  Q_OBJECT

public:
  explicit LauncherCore(QObject *parent = nullptr);
  ~LauncherCore() override;

  // Sub-systems (owned, exposed for UI wiring)
  DownloadManager *downloads() const { return m_downloads.get(); }
  ModManager *mods() const { return m_mods.get(); }
  AuthManager *auth() const { return m_auth.get(); }

  QString minecraftDir() const { return m_mcDir; }

  // ── Version management ───────────────────────────────
  void fetchVersionManifest(); // async
  std::vector<VersionEntry> cachedVersions() const { return m_versions; }

  // ── Install → uses DownloadManager worker pool ───────
  void installVersion(const QString &versionId); // async – delta

  // ── Launch ───────────────────────────────────────────
  void launchGame(const QString &versionId, int ramMb,
                  const QString &profileName = "");

  // ── Installed versions ───────────────────────────────
  QStringList installedVersionIds() const;

signals:
  void versionsReady(QStringList ids);
  void installProgress(int done, int total, QString file);
  void installFinished(bool ok, QString msg);
  void gameStarted();
  void gameClosed(int exitCode);

private:
  QString m_mcDir;

  std::unique_ptr<DownloadManager> m_downloads;
  std::unique_ptr<ModManager> m_mods;
  std::unique_ptr<AuthManager> m_auth;

  std::vector<VersionEntry> m_versions;

  // helpers
  void doInstall(const QString &versionId);
};
