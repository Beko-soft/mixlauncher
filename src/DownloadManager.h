#pragma once

#include <QMutex>
#include <QObject>
#include <QString>
#include <QTimer>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <thread>
#include <vector>

// Görev yapısı
struct DownloadTask {
  std::string url;
  std::string destPath;
  std::string expectedSha1;
  long long expectedSize = 0;
  int retries = 0;
};

class DownloadManager : public QObject {
  Q_OBJECT

public:
  explicit DownloadManager(int workerCount = 16, QObject *parent = nullptr);
  ~DownloadManager() override;

  void enqueue(std::shared_ptr<DownloadTask> task);
  void enqueueBatch(const std::vector<std::shared_ptr<DownloadTask>> &tasks);

  void start();
  void cancel();
  void waitUntilDone();

  bool isRunning() const { return m_running.load(); }

signals:
  void progressUpdated(int done, int total, QString currentFile);
  void allFinished(int success, int failed);

private:
  void workerLoop();
  bool downloadOne(const DownloadTask &task);
  std::string computeSha1(const std::string &filePath);
  bool verifySha1(const std::string &filePath, const std::string &expected);
  void pollProgress(); // Main thread poll

  // Kuyruk
  std::deque<std::shared_ptr<DownloadTask>> m_queue;
  std::mutex m_queueMtx;
  std::condition_variable m_queueCv;

  // Workerlar
  std::vector<std::thread> m_workers;
  int m_workerCount;
  std::atomic<bool> m_running{false};
  std::atomic<bool> m_cancelled{false};

  // İstatistik
  std::atomic<int> m_totalCount{0};
  std::atomic<int> m_completedCount{0};
  std::atomic<int> m_failedCount{0};

  // UI için son dosya bilgisi
  std::string m_currentFile;
  std::mutex m_currentFileMtx;

  // Qt Timer
  std::unique_ptr<QTimer> m_pollTimer;
};
