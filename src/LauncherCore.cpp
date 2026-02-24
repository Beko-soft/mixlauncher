#include "LauncherCore.h"
#include "AuthManager.h"
#include "DownloadManager.h"
#include "ModManager.h"

#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <QDir>
#include <QProcess>
#include <QStandardPaths>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;
static const char *UA = "MixCrafter/2.0";

// ══════════════════════════════════════════════════════════
LauncherCore::LauncherCore(QObject *parent) : QObject(parent) {
#ifdef Q_OS_WIN
  m_mcDir = QDir::homePath() + "/AppData/Roaming/.minecraftmix";
#else
  m_mcDir = QDir::homePath() + "/.minecraftmix";
#endif
  QDir().mkpath(m_mcDir);
  QDir().mkpath(m_mcDir + "/versions");
  QDir().mkpath(m_mcDir + "/libraries");
  QDir().mkpath(m_mcDir + "/assets");

  // Use a reasonable number of threads to avoid UI freezes
  m_downloads = std::make_unique<DownloadManager>(12, this);
  m_mods = std::make_unique<ModManager>(m_mcDir, this);
  m_auth = std::make_unique<AuthManager>(m_mcDir, this);

  // Wire download signals → our signals
  connect(m_downloads.get(), &DownloadManager::progressUpdated, this,
          &LauncherCore::installProgress);
  connect(m_downloads.get(), &DownloadManager::allFinished, this,
          [this](int ok, int fail) {
            emit installFinished(
                fail == 0, fail == 0
                               ? "Kurulum tamamlandi!"
                               : QString("%1 dosya indirilemedi").arg(fail));
          });

  // Download authlib-injector on startup (async, non-blocking)
  m_auth->ensureAuthlibInjector();
}

LauncherCore::~LauncherCore() = default;

// ══════════════════════════════════════════════════════════
//  Version Manifest
// ══════════════════════════════════════════════════════════
void LauncherCore::fetchVersionManifest() {
  std::thread([this]() {
    auto r = cpr::Get(
        cpr::Url{
            "https://piston-meta.mojang.com/mc/game/version_manifest_v2.json"},
        cpr::Header{{"User-Agent", UA}}, cpr::Timeout{15000});

    if (r.status_code != 200) {
      spdlog::error("Manifest alinamadi: HTTP {}", r.status_code);
      return;
    }

    // ── Async parse ─────────────────────────────────
    auto j = json::parse(r.text, nullptr, false);
    if (j.is_discarded()) {
      spdlog::error("Manifest parse hatasi");
      return;
    }

    m_versions.clear();
    QStringList ids;
    for (auto &v : j["versions"]) {
      VersionEntry e;
      e.id = v.value("id", "");
      e.type = v.value("type", "");
      e.url = v.value("url", "");
      m_versions.push_back(e);
      if (e.type == "release")
        ids << QString::fromStdString(e.id);
    }

    spdlog::info("Manifest: {} release surumu yuklendi", ids.size());
    emit versionsReady(ids);
  }).detach();
}

// ══════════════════════════════════════════════════════════
//  Install Version  (full delta – libraries + assets + jar)
// ══════════════════════════════════════════════════════════
void LauncherCore::installVersion(const QString &versionId) {
  std::thread([this, versionId]() { doInstall(versionId); }).detach();
}

void LauncherCore::doInstall(const QString &versionId) {
  std::string vid = versionId.toStdString();

  // 1. Find version url
  std::string vUrl;
  for (auto &v : m_versions)
    if (v.id == vid) {
      vUrl = v.url;
      break;
    }

  if (vUrl.empty()) {
    // Might be a modded version (Fabric etc.) – try local json
    std::string localJson =
        m_mcDir.toStdString() + "/versions/" + vid + "/" + vid + ".json";
    if (fs::exists(localJson)) {
      spdlog::info("Yerel JSON kullaniliyor: {}", vid);
      // For modded versions, we mainly need the parent (inheritsFrom) installed
      std::ifstream ifs(localJson);
      auto jLocal = json::parse(ifs, nullptr, false);
      if (!jLocal.is_discarded() && jLocal.contains("inheritsFrom")) {
        std::string parent = jLocal["inheritsFrom"];
        doInstall(QString::fromStdString(parent)); // install parent first
      }
      emit installFinished(true, "Modlu surum hazir");
      return;
    }
    emit installFinished(false, "Surum bulunamadi: " + versionId);
    return;
  }

  // 2. Download version JSON
  std::cout << "[INFO] Sürüm JSON indiriliyor: " << vUrl << std::endl;
  auto vr = cpr::Get(cpr::Url{vUrl}, cpr::Header{{"User-Agent", UA}},
                     cpr::VerifySsl{false});
  if (vr.status_code != 200) {
    std::cerr << "[HATA] Sürüm JSON indirilemedi: " << vr.status_code
              << std::endl;
    emit installFinished(false, "Version JSON indirilemedi");
    return;
  }

  auto vj = json::parse(vr.text, nullptr, false);
  if (vj.is_discarded()) {
    std::cerr << "[HATA] JSON parse hatası" << std::endl;
    emit installFinished(false, "JSON parse hatasi");
    return;
  }

  // Save version json
  std::string verDir = m_mcDir.toStdString() + "/versions/" + vid;
  fs::create_directories(verDir);
  {
    std::ofstream ofs(verDir + "/" + vid + ".json");
    ofs << vj.dump(2);
  }

  // 3. Build download queue
  std::vector<std::shared_ptr<DownloadTask>> tasks;

  // 3a. Client JAR
  if (vj.contains("downloads") && vj["downloads"].contains("client")) {
    auto t = std::make_shared<DownloadTask>();
    t->url = vj["downloads"]["client"]["url"];
    t->expectedSha1 = vj["downloads"]["client"].value("sha1", "");
    t->expectedSize = vj["downloads"]["client"].value("size", 0);
    t->destPath = verDir + "/" + vid + ".jar";
    tasks.push_back(t);
  }

  // 3b. Libraries
  if (vj.contains("libraries")) {
    for (auto &lib : vj["libraries"]) {
      if (!lib.contains("downloads"))
        continue;

      // Check rules (OS filtering)
      bool allowed = true;
      if (lib.contains("rules")) {
        allowed = false;
        for (auto &rule : lib["rules"]) {
          std::string action = rule.value("action", "allow");
          if (rule.contains("os")) {
            std::string osName = rule["os"].value("name", "");
            if (osName == "linux" && action == "allow")
              allowed = true;
            if (osName == "linux" && action == "disallow")
              allowed = false;
          } else {
            allowed = (action == "allow");
          }
        }
      }
      if (!allowed)
        continue;

      if (lib["downloads"].contains("artifact")) {
        auto &art = lib["downloads"]["artifact"];
        auto t = std::make_shared<DownloadTask>();
        t->url = art.value("url", "");
        t->expectedSha1 = art.value("sha1", "");
        t->expectedSize = art.value("size", 0);
        t->destPath =
            m_mcDir.toStdString() + "/libraries/" + art.value("path", "");
        if (!t->url.empty())
          tasks.push_back(t);
      }
    }
  }

  // 3c. Asset index
  if (vj.contains("assetIndex")) {
    auto &ai = vj["assetIndex"];
    auto t = std::make_shared<DownloadTask>();
    t->url = ai.value("url", "");
    t->expectedSha1 = ai.value("sha1", "");
    std::string assetId = ai.value("id", "");
    t->destPath =
        m_mcDir.toStdString() + "/assets/indexes/" + assetId + ".json";
    tasks.push_back(t);

    // Download the asset index to also queue individual assets
    auto air = cpr::Get(cpr::Url{t->url}, cpr::Header{{"User-Agent", UA}});
    if (air.status_code == 200) {
      // Save index
      fs::create_directories(fs::path(t->destPath).parent_path());
      {
        std::ofstream ofs(t->destPath);
        ofs << air.text;
      }

      auto aj = json::parse(air.text, nullptr, false);
      if (!aj.is_discarded() && aj.contains("objects")) {
        for (auto &[key, obj] : aj["objects"].items()) {
          std::string hash = obj.value("hash", "");
          if (hash.size() < 2)
            continue;
          std::string prefix = hash.substr(0, 2);
          auto at = std::make_shared<DownloadTask>();
          at->url =
              "https://resources.download.minecraft.net/" + prefix + "/" + hash;
          at->expectedSha1 = hash;
          at->destPath =
              m_mcDir.toStdString() + "/assets/objects/" + prefix + "/" + hash;
          tasks.push_back(at);
        }
      }
    }
  }

  std::cout << "[INFO] İndirme kuyruğu oluşturuldu: " << tasks.size()
            << " dosya" << std::endl;
  spdlog::info("Indirme kuyruğu: {} dosya", tasks.size());

  // 4. Feed to DownloadManager and start
  m_downloads->enqueueBatch(std::move(tasks));
  m_downloads->start();
}

// ══════════════════════════════════════════════════════════
//  Helper: Maven name → library path  (e.g. "net.fabricmc:fabric-loader:0.18.4"
//  → "net/fabricmc/fabric-loader/0.18.4/fabric-loader-0.18.4.jar")
// ══════════════════════════════════════════════════════════
static std::string mavenToPath(const std::string &name) {
  // format: group:artifact:version[:classifier]
  std::vector<std::string> parts;
  std::string token;
  for (char c : name) {
    if (c == ':') {
      parts.push_back(token);
      token.clear();
    } else
      token += c;
  }
  parts.push_back(token);
  if (parts.size() < 3)
    return "";

  std::string group = parts[0];
  for (auto &ch : group)
    if (ch == '.')
      ch = '/';
  std::string artifact = parts[1];
  std::string version = parts[2];
  std::string classifier = parts.size() > 3 ? ("-" + parts[3]) : "";

  return group + "/" + artifact + "/" + version + "/" + artifact + "-" +
         version + classifier + ".jar";
}

// ══════════════════════════════════════════════════════════
//  Launch Game  (with full inheritsFrom / Fabric / Quilt / Forge support)
// ══════════════════════════════════════════════════════════
void LauncherCore::launchGame(const QString &versionId, int ramMb,
                              const QString &profileName) {
  auto session = m_auth->currentSession();
  if (!session.valid) {
    spdlog::warn("Giris yapilmadan oyun baslatiliyor (offline)");
    m_auth->loginOffline("Player");
    session = m_auth->currentSession();
  }

  std::string vid = versionId.toStdString();

  // Mod loader check — find the actual modded version ID on disk
  QString loader;
  for (auto &p : m_mods->listProfiles()) {
    if (p.name == profileName) {
      loader = p.loader;
      break;
    }
  }

  if (loader == "fabric") {
    for (auto entry : QDir(m_mcDir + "/versions")
                          .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      if (entry.startsWith("fabric-loader-") &&
          entry.endsWith("-" + versionId)) {
        vid = entry.toStdString();
        break;
      }
    }
  } else if (loader == "quilt") {
    for (auto entry : QDir(m_mcDir + "/versions")
                          .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      if (entry.startsWith("quilt-loader-") &&
          entry.endsWith("-" + versionId)) {
        vid = entry.toStdString();
        break;
      }
    }
  } else if (loader == "forge") {
    for (auto entry : QDir(m_mcDir + "/versions")
                          .entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
      if (entry.contains("forge") && entry.contains(versionId)) {
        vid = entry.toStdString();
        break;
      }
    }
  }

  std::string verDir = m_mcDir.toStdString() + "/versions/" + vid;
  std::string jsonPath = verDir + "/" + vid + ".json";

  if (!fs::exists(jsonPath)) {
    spdlog::error("Version JSON bulunamadi: {}", jsonPath);
    return;
  }

  std::ifstream ifs(jsonPath);
  auto vj = json::parse(ifs, nullptr, false);
  if (vj.is_discarded())
    return;

  // ── Handle inheritsFrom (Fabric / Quilt / Forge) ──
  json parentJson;
  bool hasParent = false;
  std::string parentVid;
  if (vj.contains("inheritsFrom")) {
    parentVid = vj["inheritsFrom"].get<std::string>();
    std::string parentPath = m_mcDir.toStdString() + "/versions/" + parentVid +
                             "/" + parentVid + ".json";
    if (fs::exists(parentPath)) {
      std::ifstream pifs(parentPath);
      parentJson = json::parse(pifs, nullptr, false);
      hasParent = !parentJson.is_discarded();
    }
    if (!hasParent) {
      spdlog::error("Parent version JSON bulunamadi: {}", parentVid);
      return;
    }
  }

  // ── Build classpath ──
  std::string cp;
  auto addToClasspath = [&](const std::string &path) {
    if (path.empty())
      return;
    if (!cp.empty())
      cp += ":";
    cp += path;
  };

  // 1) Parent (vanilla) libraries first
  if (hasParent && parentJson.contains("libraries")) {
    for (auto &lib : parentJson["libraries"]) {
      if (!lib.contains("downloads") || !lib["downloads"].contains("artifact"))
        continue;

      // OS filter
      bool allowed = true;
      if (lib.contains("rules")) {
        allowed = false;
        for (auto &rule : lib["rules"]) {
          std::string action = rule.value("action", "allow");
          if (rule.contains("os")) {
            std::string osName = rule["os"].value("name", "");
            if (osName == "linux" && action == "allow")
              allowed = true;
            if (osName == "linux" && action == "disallow")
              allowed = false;
          } else {
            allowed = (action == "allow");
          }
        }
      }
      if (!allowed)
        continue;

      std::string path = lib["downloads"]["artifact"].value("path", "");
      addToClasspath(m_mcDir.toStdString() + "/libraries/" + path);
    }
  }

  // 2) Modded version libraries
  if (vj.contains("libraries")) {
    for (auto &lib : vj["libraries"]) {
      // Fabric/Quilt libraries use "name" (Maven coord) with "url" but NO
      // "downloads.artifact"
      if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
        std::string path = lib["downloads"]["artifact"].value("path", "");
        addToClasspath(m_mcDir.toStdString() + "/libraries/" + path);
      } else if (lib.contains("name")) {
        // Maven-style library (Fabric / Quilt)
        std::string mavenName = lib["name"].get<std::string>();
        std::string path = mavenToPath(mavenName);
        if (!path.empty())
          addToClasspath(m_mcDir.toStdString() + "/libraries/" + path);
      }
    }
  }

  // 3) Client JAR — use parent's if inheritsFrom
  if (hasParent) {
    std::string parentVerDir = m_mcDir.toStdString() + "/versions/" + parentVid;
    addToClasspath(parentVerDir + "/" + parentVid + ".jar");
  } else {
    addToClasspath(verDir + "/" + vid + ".jar");
  }

  // ── Main class ──
  std::string mainClass =
      vj.value("mainClass", "net.minecraft.client.main.Main");

  // ── Asset index — prefer parent's ──
  std::string assetIndex = "legacy";
  if (hasParent && parentJson.contains("assetIndex"))
    assetIndex = parentJson["assetIndex"].value("id", "legacy");
  else if (vj.contains("assetIndex"))
    assetIndex = vj["assetIndex"].value("id", "legacy");

  // ── Natives dir — use parent's for vanilla natives ──
  std::string nativesDir = verDir + "/natives";
  if (hasParent) {
    std::string parentNatives =
        m_mcDir.toStdString() + "/versions/" + parentVid + "/natives";
    if (fs::exists(parentNatives))
      nativesDir = parentNatives;
  }

  // ── Build command ──
  QStringList args;

  // JVM args
  args << QString("-Xmx%1M").arg(ramMb);
  args << "-Xms512M";
  args << QString("-Djava.library.path=%1")
              .arg(QString::fromStdString(nativesDir));

  // AuthLib-injector for Ely.by
  args << m_auth->jvmArgsForElyBy();

  args << "-cp" << QString::fromStdString(cp);
  args << QString::fromStdString(mainClass);

  // Game args
  args << "--username" << session.username;
  args << "--uuid" << session.uuid;
  args << "--accessToken"
       << (session.accessToken.isEmpty() ? "0" : session.accessToken);
  args << "--version" << QString::fromStdString(vid);
  args << "--gameDir" << m_mcDir;
  args << "--assetsDir" << (m_mcDir + "/assets");
  args << "--assetIndex" << QString::fromStdString(assetIndex);

  // If a profile specifies a custom mods dir, set gameDir
  if (!profileName.isEmpty()) {
    QString modsPath = m_mods->profileModsPath(profileName);
    QDir().mkpath(modsPath);
    for (int i = 0; i < args.size(); ++i) {
      if (args[i] == "--gameDir" && i + 1 < args.size()) {
        QString profRoot = QDir(modsPath).filePath("..");
        args[i + 1] = QDir::cleanPath(profRoot);
      }
    }
  }

  spdlog::info("Oyun baslatiliyor: java {}", args.join(" ").toStdString());

  auto *proc = new QProcess(this);
  connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
          this, [this, proc](int code, QProcess::ExitStatus) {
            emit gameClosed(code);
            proc->deleteLater();
          });

  proc->start("java", args);
  emit gameStarted();
}

// ══════════════════════════════════════════════════════════
QStringList LauncherCore::installedVersionIds() const {
  QStringList ids;
  QString versionsDir = m_mcDir + "/versions";
  QDir dir(versionsDir);
  for (auto &entry : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
    if (QFile::exists(versionsDir + "/" + entry + "/" + entry + ".json"))
      ids << entry;
  }
  return ids;
}
