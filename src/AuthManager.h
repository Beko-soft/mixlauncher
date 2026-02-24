#pragma once

#include <QObject>
#include <QPixmap>
#include <QString>

// ── Session ──────────────────────────────────────────────
struct AuthSession {
  QString username;
  QString uuid;
  QString accessToken;
  QString clientToken;
  QString authType; // "elyby" | "offline"
  QString skinUrl;
  QString capeUrl;
  bool valid = false;
};

// ── Auth Manager ─────────────────────────────────────────
class AuthManager : public QObject {
  Q_OBJECT

public:
  explicit AuthManager(const QString &dataDir, QObject *parent = nullptr);

  // Ely.by authentication
  void loginElyBy(const QString &username, const QString &password);

  // Offline (cracked) mode
  void loginOffline(const QString &username);

  // Session
  AuthSession currentSession() const { return m_session; }
  bool isLoggedIn() const { return m_session.valid; }
  void logout();

  // AuthLib-Injector helpers
  void ensureAuthlibInjector();        // download jar if missing
  QStringList jvmArgsForElyBy() const; // -javaagent:...
  QString authlibJarPath() const;

  // Skin fetching
  void fetchSkin(const QString &username);

signals:
  void loginSuccess(AuthSession session);
  void loginFailed(QString error);
  void skinReady(QPixmap skin);

private:
  AuthSession m_session;
  QString m_dataDir;
  QString m_authlibPath;

  static QString generateClientToken();
  static QString offlineUuid(const QString &name);
};
