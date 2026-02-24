#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <memory>

// ── Data structures ──────────────────────────────────────
struct ModSearchResult {
  QString title;
  QString author;
  QString description;
  QString projectId;
  QString slug;
  QString iconUrl;
  int downloads = 0;
};

struct ModDependency {
  QString projectId;
  QString versionId;
  QString fileName;
  QString depType; // "required" | "optional"
};

struct ModProfile {
  QString name;
  QString gameVersion;
  QString loader;   // "fabric" | "forge" | "quilt" | "vanilla"
  QString modsPath; // absolute path to isolated mods folder
};

// ── Mod Manager ──────────────────────────────────────────
class ModManager : public QObject {
  Q_OBJECT

public:
  explicit ModManager(const QString &mcDir, QObject *parent = nullptr);

  // ── Search ───────────────────────────────────────────
  void searchMods(const QString &query, const QString &loader,
                  const QString &gameVersion,
                  const QString &projectType = "mod");

  // ── Install (with auto-dependency resolution) ────────
  void installMod(const QString &projectId, const QString &loader,
                  const QString &gameVersion, const QString &profileName);

  // ── Loader installation ──────────────────────────────
  void installFabric(const QString &gameVersion);
  void installQuilt(const QString &gameVersion);
  void installForge(const QString &gameVersion);

  // ── Profile management ───────────────────────────────
  void createProfile(const QString &name, const QString &gameVer,
                     const QString &loader);
  QVector<ModProfile> listProfiles() const;
  QString profileModsPath(const QString &name) const;
  void deleteProfile(const QString &name);

signals:
  void searchFinished(QVector<ModSearchResult> results);
  void modInstalled(QString modName, bool success);
  void dependenciesFound(QVector<ModDependency> deps, QString parentMod);
  void loaderInstalled(QString loaderName, QString versionId, bool ok);
  void profilesChanged();

private:
  void resolveAndInstall(const QString &projectId, const QString &loader,
                         const QString &gameVersion, const QString &modsPath);

  QString m_mcDir;
  QVector<ModProfile> m_profiles;
  void loadProfiles();
  void saveProfiles();
};
