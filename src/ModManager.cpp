#include "ModManager.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

#include <filesystem>
#include <fstream>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;

static const char *MODRINTH = "https://api.modrinth.com/v2";
static const char *FABRIC_META = "https://meta.fabricmc.net/v2";
static const char *QUILT_META = "https://meta.quiltmc.org/v3";
static const char *UA = "MixCrafter/2.0 (contact@mixcrafter)";

// ══════════════════════════════════════════════════════════
ModManager::ModManager(const QString &mcDir, QObject *parent)
    : QObject(parent), m_mcDir(mcDir) {
  loadProfiles();
}

// ══════════════════════════════════════════════════════════
//  Profile persistence  (JSON in mcDir/profiles.json)
// ══════════════════════════════════════════════════════════
void ModManager::loadProfiles() {
  QString path = m_mcDir + "/profiles.json";
  QFile f(path);
  if (!f.open(QIODevice::ReadOnly))
    return;
  QJsonDocument doc = QJsonDocument::fromJson(f.readAll());
  for (auto v : doc.array()) {
    auto o = v.toObject();
    ModProfile p;
    p.name = o["name"].toString();
    p.gameVersion = o["gameVersion"].toString();
    p.loader = o["loader"].toString();
    p.modsPath = o["modsPath"].toString();
    m_profiles.push_back(p);
  }
}

void ModManager::saveProfiles() {
  QJsonArray arr;
  for (auto &p : m_profiles) {
    QJsonObject o;
    o["name"] = p.name;
    o["gameVersion"] = p.gameVersion;
    o["loader"] = p.loader;
    o["modsPath"] = p.modsPath;
    arr.append(o);
  }
  QFile f(m_mcDir + "/profiles.json");
  if (f.open(QIODevice::WriteOnly))
    f.write(QJsonDocument(arr).toJson());
}

void ModManager::createProfile(const QString &name, const QString &gameVer,
                               const QString &loader) {
  ModProfile p;
  p.name = name;
  p.gameVersion = gameVer;
  p.loader = loader;
  p.modsPath = m_mcDir + "/profiles/" + name + "/mods";
  QDir().mkpath(p.modsPath);
  m_profiles.push_back(p);
  saveProfiles();
  emit profilesChanged();
  spdlog::info("Profil olusturuldu: {}", name.toStdString());
}

QVector<ModProfile> ModManager::listProfiles() const { return m_profiles; }

QString ModManager::profileModsPath(const QString &name) const {
  for (auto &p : m_profiles)
    if (p.name == name)
      return p.modsPath;
  return m_mcDir + "/mods"; // fallback
}

void ModManager::deleteProfile(const QString &name) {
  m_profiles.erase(
      std::remove_if(m_profiles.begin(), m_profiles.end(),
                     [&](const ModProfile &p) { return p.name == name; }),
      m_profiles.end());
  saveProfiles();
  emit profilesChanged();
}

// ══════════════════════════════════════════════════════════
//  Modrinth Search
// ══════════════════════════════════════════════════════════
void ModManager::searchMods(const QString &query, const QString &loader,
                            const QString &gameVersion,
                            const QString &projectType) {
  std::string q = query.toStdString();
  std::string l = loader.toStdString();
  std::string v = gameVersion.toStdString();
  std::string pt = projectType.toStdString();

  std::thread([this, q, l, v, pt]() {
    std::string facets =
        "[[\"versions:" + v + "\"],[\"project_type:" + pt + "\"]";
    if (pt == "mod" && !l.empty() && l != "yok (vanilla)" && l != "vanilla" &&
        l != "Yok (Vanilla)") {
      facets += ",[\"categories:" + l + "\"]";
    }
    facets += "]";

    auto r = cpr::Get(
        cpr::Url{std::string(MODRINTH) + "/search"},
        cpr::Parameters{{"query", q}, {"facets", facets}, {"limit", "30"}},
        cpr::Header{{"User-Agent", UA}}, cpr::VerifySsl{false});

    QVector<ModSearchResult> results;
    if (r.status_code == 200) {
      auto j = json::parse(r.text, nullptr, false);
      if (!j.is_discarded()) {
        for (auto &hit : j["hits"]) {
          ModSearchResult m;
          m.title = QString::fromStdString(hit.value("title", ""));
          m.author = QString::fromStdString(hit.value("author", ""));
          m.description = QString::fromStdString(hit.value("description", ""));
          m.projectId = QString::fromStdString(hit.value("project_id", ""));
          m.slug = QString::fromStdString(hit.value("slug", ""));
          m.iconUrl = QString::fromStdString(hit.value("icon_url", ""));
          m.downloads = hit.value("downloads", 0);
          results.push_back(m);
        }
      }
    }
    spdlog::info("Modrinth arama: '{}' → {} sonuc", q, results.size());
    emit searchFinished(results);
  }).detach();
}

// ══════════════════════════════════════════════════════════
//  Install Mod  (with recursive dependency resolution)
// ══════════════════════════════════════════════════════════
void ModManager::installMod(const QString &projectId, const QString &loader,
                            const QString &gameVersion,
                            const QString &profileName) {
  QString modsDir = profileModsPath(profileName);
  std::thread([this, projectId, loader, gameVersion, modsDir]() {
    resolveAndInstall(projectId, loader, gameVersion, modsDir);
  }).detach();
}

void ModManager::resolveAndInstall(const QString &projectId,
                                   const QString &loader,
                                   const QString &gameVersion,
                                   const QString &modsPath) {
  std::string pid = projectId.toStdString();
  std::string l = loader.toStdString();
  std::string gv = gameVersion.toStdString();

  // 1.  Get compatible version of this mod
  auto r =
      cpr::Get(cpr::Url{std::string(MODRINTH) + "/project/" + pid + "/version"},
               cpr::Parameters{{"loaders", "[\"" + l + "\"]"},
                               {"game_versions", "[\"" + gv + "\"]"}},
               cpr::Header{{"User-Agent", UA}}, cpr::VerifySsl{false});

  if (r.status_code != 200) {
    emit modInstalled(projectId, false);
    return;
  }

  auto versions = json::parse(r.text, nullptr, false);
  if (versions.is_discarded() || versions.empty()) {
    emit modInstalled(projectId, false);
    return;
  }

  auto &ver = versions[0];

  // 2.  Check dependencies
  QVector<ModDependency> deps;
  if (ver.contains("dependencies")) {
    for (auto &dep : ver["dependencies"]) {
      std::string dt = dep.value("dependency_type", "");
      if (dt == "required") {
        ModDependency md;
        md.projectId = QString::fromStdString(dep.value("project_id", ""));
        md.versionId = QString::fromStdString(dep.value("version_id", ""));
        md.depType = "required";
        if (!md.projectId.isEmpty())
          deps.push_back(md);
      }
    }
  }

  if (!deps.isEmpty()) {
    // Signal parent mod name for UI
    QString parentTitle = projectId;
    emit dependenciesFound(deps, parentTitle);

    // Auto-install required dependencies
    for (auto &dep : deps) {
      resolveAndInstall(dep.projectId, loader, gameVersion, modsPath);
    }
  }

  // 3.  Download the jar file
  if (ver["files"].empty()) {
    emit modInstalled(projectId, false);
    return;
  }
  std::string fileUrl = ver["files"][0]["url"];
  std::string fileName = ver["files"][0]["filename"];

  fs::create_directories(modsPath.toStdString());
  std::string dest = modsPath.toStdString() + "/" + fileName;

  // Skip if already exists
  if (fs::exists(dest)) {
    spdlog::info("Mod zaten var: {}", fileName);
    emit modInstalled(QString::fromStdString(fileName), true);
    return;
  }

  auto dr = cpr::Get(cpr::Url{fileUrl}, cpr::Header{{"User-Agent", UA}},
                     cpr::Timeout{60000}, cpr::VerifySsl{false});
  if (dr.status_code == 200) {
    std::ofstream ofs(dest, std::ios::binary);
    ofs.write(dr.text.data(), static_cast<std::streamsize>(dr.text.size()));
    spdlog::info("Mod yuklendi: {}", fileName);
    emit modInstalled(QString::fromStdString(fileName), true);
  } else {
    emit modInstalled(QString::fromStdString(fileName), false);
  }
}

// ══════════════════════════════════════════════════════════
//  Fabric installer  (Meta-API → profile json)
// ══════════════════════════════════════════════════════════
void ModManager::installFabric(const QString &gameVersion) {
  std::string gv = gameVersion.toStdString();
  std::thread([this, gv]() {
    // 1. Get latest loader version
    auto lr =
        cpr::Get(cpr::Url{std::string(FABRIC_META) + "/versions/loader/" + gv},
                 cpr::Header{{"User-Agent", UA}}, cpr::VerifySsl{false});

    if (lr.status_code != 200 || lr.text.empty()) {
      emit loaderInstalled("Fabric", "", false);
      return;
    }

    auto loaders = json::parse(lr.text, nullptr, false);
    if (loaders.is_discarded() || loaders.empty()) {
      emit loaderInstalled("Fabric", "", false);
      return;
    }

    std::string loaderVer = loaders[0]["loader"]["version"];

    // 2. Get profile JSON
    std::string profileUrl = std::string(FABRIC_META) + "/versions/loader/" +
                             gv + "/" + loaderVer + "/profile/json";
    auto pr = cpr::Get(cpr::Url{profileUrl}, cpr::Header{{"User-Agent", UA}},
                       cpr::VerifySsl{false});

    if (pr.status_code != 200) {
      emit loaderInstalled("Fabric", "", false);
      return;
    }

    auto profile = json::parse(pr.text, nullptr, false);
    std::string verId =
        profile.value("id", "fabric-loader-" + loaderVer + "-" + gv);

    // 3. Write to versions/<id>/<id>.json
    std::string verDir = m_mcDir.toStdString() + "/versions/" + verId;
    fs::create_directories(verDir);
    std::ofstream ofs(verDir + "/" + verId + ".json");
    ofs << profile.dump(2);
    ofs.close();

    // 4. Download Fabric libraries (Maven jars)
    if (profile.contains("libraries")) {
      for (auto &lib : profile["libraries"]) {
        if (!lib.contains("name"))
          continue;
        std::string mavenName = lib["name"].get<std::string>();

        // Parse Maven coordinate → path
        std::vector<std::string> parts;
        std::string tok;
        for (char c : mavenName) {
          if (c == ':') {
            parts.push_back(tok);
            tok.clear();
          } else
            tok += c;
        }
        parts.push_back(tok);
        if (parts.size() < 3)
          continue;

        std::string group = parts[0];
        for (auto &ch : group)
          if (ch == '.')
            ch = '/';
        std::string artifact = parts[1];
        std::string version = parts[2];
        std::string jarPath = group + "/" + artifact + "/" + version + "/" +
                              artifact + "-" + version + ".jar";

        std::string destPath = m_mcDir.toStdString() + "/libraries/" + jarPath;

        // Skip if already exists
        if (fs::exists(destPath))
          continue;

        // Determine URL from lib's "url" field or default Maven
        std::string baseUrl = lib.value("url", "https://maven.fabricmc.net/");
        if (!baseUrl.empty() && baseUrl.back() != '/')
          baseUrl += '/';
        std::string downloadUrl = baseUrl + jarPath;

        spdlog::info("Fabric lib indiriliyor: {}", downloadUrl);
        fs::create_directories(fs::path(destPath).parent_path());

        auto dr =
            cpr::Get(cpr::Url{downloadUrl}, cpr::Header{{"User-Agent", UA}},
                     cpr::Timeout{60000}, cpr::VerifySsl{false});
        if (dr.status_code == 200) {
          std::ofstream libOfs(destPath, std::ios::binary);
          libOfs.write(dr.text.data(),
                       static_cast<std::streamsize>(dr.text.size()));
          spdlog::info("Fabric lib yuklendi: {}", artifact);
        } else {
          spdlog::warn("Fabric lib indirilemedi: {} (HTTP {})", artifact,
                       dr.status_code);
        }
      }
    }

    spdlog::info("Fabric kuruldu: {}", verId);
    emit loaderInstalled("Fabric", QString::fromStdString(verId), true);
  }).detach();
}

// ══════════════════════════════════════════════════════════
//  Quilt installer
// ══════════════════════════════════════════════════════════
void ModManager::installQuilt(const QString &gameVersion) {
  std::string gv = gameVersion.toStdString();
  std::thread([this, gv]() {
    auto lr =
        cpr::Get(cpr::Url{std::string(QUILT_META) + "/versions/loader/" + gv},
                 cpr::Header{{"User-Agent", UA}}, cpr::VerifySsl{false});

    if (lr.status_code != 200) {
      emit loaderInstalled("Quilt", "", false);
      return;
    }

    auto loaders = json::parse(lr.text, nullptr, false);
    if (loaders.is_discarded() || loaders.empty()) {
      emit loaderInstalled("Quilt", "", false);
      return;
    }

    std::string loaderVer = loaders[0]["loader"]["version"];
    std::string profileUrl = std::string(QUILT_META) + "/versions/loader/" +
                             gv + "/" + loaderVer + "/profile/json";
    auto pr = cpr::Get(cpr::Url{profileUrl}, cpr::Header{{"User-Agent", UA}},
                       cpr::VerifySsl{false});

    if (pr.status_code != 200) {
      emit loaderInstalled("Quilt", "", false);
      return;
    }

    auto profile = json::parse(pr.text, nullptr, false);
    std::string verId =
        profile.value("id", "quilt-loader-" + loaderVer + "-" + gv);

    std::string verDir = m_mcDir.toStdString() + "/versions/" + verId;
    fs::create_directories(verDir);
    std::ofstream ofs(verDir + "/" + verId + ".json");
    ofs << profile.dump(2);

    spdlog::info("Quilt kuruldu: {}", verId);
    emit loaderInstalled("Quilt", QString::fromStdString(verId), true);
  }).detach();
}

// ══════════════════════════════════════════════════════════
//  Forge installer  (headless java -jar)
// ══════════════════════════════════════════════════════════
void ModManager::installForge(const QString &gameVersion) {
  std::string gv = gameVersion.toStdString();
  std::thread([this, gv]() {
    // 1. Find forge version via promotions
    auto pr = cpr::Get(cpr::Url{"https://files.minecraftforge.net/net/"
                                "minecraftforge/forge/promotions_slim.json"},
                       cpr::Header{{"User-Agent", UA}}, cpr::VerifySsl{false});

    if (pr.status_code != 200) {
      emit loaderInstalled("Forge", "", false);
      return;
    }

    auto promos = json::parse(pr.text, nullptr, false);
    std::string key = gv + "-recommended";
    std::string forgeVer;

    if (promos.contains("promos") && promos["promos"].contains(key)) {
      forgeVer = promos["promos"][key].get<std::string>();
    } else {
      // try latest
      key = gv + "-latest";
      if (promos.contains("promos") && promos["promos"].contains(key))
        forgeVer = promos["promos"][key].get<std::string>();
    }

    if (forgeVer.empty()) {
      spdlog::warn("Forge bulunamadi: {}", gv);
      emit loaderInstalled("Forge", "", false);
      return;
    }

    // 2. Download installer jar
    std::string fullVer = gv + "-" + forgeVer;
    std::string installerUrl =
        "https://maven.minecraftforge.net/net/minecraftforge/forge/" + fullVer +
        "/forge-" + fullVer + "-installer.jar";

    std::string installerPath = m_mcDir.toStdString() + "/forge-installer.jar";
    auto dr = cpr::Get(cpr::Url{installerUrl}, cpr::Header{{"User-Agent", UA}},
                       cpr::Timeout{120000}, cpr::VerifySsl{false});

    if (dr.status_code != 200) {
      spdlog::error("Forge installer indirilemedi: HTTP {}", dr.status_code);
      emit loaderInstalled("Forge", "", false);
      return;
    }

    std::ofstream ofs(installerPath, std::ios::binary);
    ofs.write(dr.text.data(), static_cast<std::streamsize>(dr.text.size()));
    ofs.close();

    // 3. Run installer headless
    QProcess proc;
    proc.setWorkingDirectory(QString::fromStdString(m_mcDir.toStdString()));
    proc.start("java", {"-jar", QString::fromStdString(installerPath),
                        "--installClient", m_mcDir});
    proc.waitForFinished(300000); // 5 min timeout

    bool ok = (proc.exitCode() == 0);
    if (ok) {
      spdlog::info("Forge kuruldu: {}", fullVer);
      fs::remove(installerPath); // cleanup
    } else {
      spdlog::error("Forge installer hata: {}",
                    proc.readAllStandardError().toStdString());
    }
    emit loaderInstalled("Forge", QString::fromStdString(fullVer), ok);
  }).detach();
}
