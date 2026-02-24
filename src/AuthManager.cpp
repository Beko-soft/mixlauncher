#include "AuthManager.h"
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include <QCryptographicHash>
#include <QDir>
#include <QPixmap>
#include <QUuid>
#include <fstream>
#include <iostream>
#include <thread>

using json = nlohmann::json;
namespace fs = std::filesystem;

// AuthLib-injector
static const char *AUTHLIB_URL =
    "https://authlib-injector.yushi.moe/artifact/latest.json";
static const char *ELYBY_AUTH = "https://authserver.ely.by/auth/authenticate";

AuthManager::AuthManager(const QString &dataDir, QObject *parent)
    : QObject(parent), m_dataDir(dataDir) {
  m_authlibPath = m_dataDir + "/authlib-injector.jar";
  QDir().mkpath(m_dataDir);

  // Başlangıçta indirilmeli
  ensureAuthlibInjector();
}

void AuthManager::loginElyBy(const QString &username, const QString &password) {
  std::string u = username.toStdString();
  std::string p = password.toStdString();
  std::string clientToken = generateClientToken().toStdString();

  std::thread([this, u, p, clientToken]() {
    try {
      json body = {{"agent", {{"name", "Minecraft"}, {"version", 1}}},
                   {"username", u},
                   {"password", p},
                   {"clientToken", clientToken},
                   {"requestUser", true}};

      auto r = cpr::Post(cpr::Url{ELYBY_AUTH},
                         cpr::Header{{"Content-Type", "application/json"},
                                     {"User-Agent", "MixLauncher/2.0"},
                                     {"Accept", "application/json"}},
                         cpr::Body{body.dump()},
                         cpr::VerifySsl{false} // Linux SSL fix
      );

      if (r.status_code == 200) {
        auto j = json::parse(r.text);
        AuthSession s;
        s.accessToken = QString::fromStdString(j.value("accessToken", ""));
        s.clientToken = QString::fromStdString(j.value("clientToken", ""));

        auto prof = j["selectedProfile"];
        s.username = QString::fromStdString(prof.value("name", "Player"));
        s.uuid = QString::fromStdString(prof.value("id", ""));
        s.authType = "elyby";
        s.valid = true;

        m_session = s;
        emit loginSuccess(s);
        spdlog::info("Ely.by Giris Basarili: {}", s.username.toStdString());
      } else {
        emit loginFailed(QString("Ely.by Hatası: HTTP %1").arg(r.status_code));
        spdlog::error("Ely.by Login Fail: {}", r.text);
      }
    } catch (const std::exception &e) {
      emit loginFailed(QString("Exception: %1").arg(e.what()));
    }
  }).detach();
}

void AuthManager::loginOffline(const QString &username) {
  AuthSession s;
  s.username = username;
  s.uuid = offlineUuid(username);
  s.valid = true;
  s.authType = "offline";
  m_session = s;
  emit loginSuccess(s);
}

void AuthManager::logout() { m_session = AuthSession{}; }

void AuthManager::fetchSkin(const QString &username) {
  std::string url =
      "http://skinsystem.ely.by/skins/" + username.toStdString() + ".png";
  std::thread([this, url]() {
    auto r = cpr::Get(cpr::Url{url}, cpr::VerifySsl{false}, cpr::Timeout{5000});
    if (r.status_code == 200 && !r.text.empty()) {
      QPixmap pm;
      if (pm.loadFromData((const uchar *)r.text.data(), r.text.size())) {
        emit skinReady(pm);
      }
    }
  }).detach();
}

void AuthManager::ensureAuthlibInjector() {
  if (fs::exists(m_authlibPath.toStdString()))
    return;

  std::thread([this]() {
    spdlog::info("AuthLib-injector meta indiriliyor...");
    auto r = cpr::Get(cpr::Url{AUTHLIB_URL}, cpr::VerifySsl{false},
                      cpr::Header{{"User-Agent", "MixLauncher/2.0"}});

    if (r.status_code != 200) {
      spdlog::error("AuthLib meta hatasi: {}", r.status_code);
      return;
    }

    try {
      auto j = json::parse(r.text);
      std::string dl = j.value("download_url", "");
      if (dl.empty())
        return;

      spdlog::info("AuthLib JAR indiriliyor: {}", dl);
      auto jar = cpr::Get(cpr::Url{dl}, cpr::VerifySsl{false});
      if (jar.status_code == 200) {
        std::ofstream ofs(m_authlibPath.toStdString(), std::ios::binary);
        ofs.write(jar.text.data(), jar.text.size());
        spdlog::info("AuthLib-injector hazir.");
      }
    } catch (...) {
    }
  }).detach();
}

QStringList AuthManager::jvmArgsForElyBy() const {
  QStringList args;
  if (m_session.authType == "elyby" &&
      fs::exists(m_authlibPath.toStdString())) {
    args << QString("-javaagent:%1=https://authserver.ely.by")
                .arg(m_authlibPath);
  }
  return args;
}

QString AuthManager::authlibJarPath() const { return m_authlibPath; }

QString AuthManager::generateClientToken() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces);
}

QString AuthManager::offlineUuid(const QString &name) {
  QByteArray hash = QCryptographicHash::hash(("OfflinePlayer:" + name).toUtf8(),
                                             QCryptographicHash::Md5);
  QString hex = hash.toHex();
  // UUID formatı (8-4-4-4-12)
  return hex.mid(0, 8) + "-" + hex.mid(8, 4) + "-" + hex.mid(12, 4) + "-" +
         hex.mid(16, 4) + "-" + hex.mid(20, 12);
}
