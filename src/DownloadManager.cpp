#include "DownloadManager.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <cpr/cpr.h>
#include <openssl/evp.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <thread>

namespace fs = std::filesystem;

DownloadManager::DownloadManager(int workerCount, QObject *parent)
    : QObject(parent), m_workerCount(workerCount) {
  if (m_workerCount < 2)
    m_workerCount = 2;
  if (m_workerCount > 32)
    m_workerCount = 32;

  m_pollTimer = std::make_unique<QTimer>(this);
  m_pollTimer->setInterval(150);
  connect(m_pollTimer.get(), &QTimer::timeout, this,
          &DownloadManager::pollProgress);
}

DownloadManager::~DownloadManager() { cancel(); }

void DownloadManager::enqueue(std::shared_ptr<DownloadTask> task) {
  std::lock_guard<std::mutex> lk(m_queueMtx);
  m_queue.push_back(std::move(task));
  m_totalCount.fetch_add(1);
  m_queueCv.notify_one();
}

void DownloadManager::enqueueBatch(
    const std::vector<std::shared_ptr<DownloadTask>> &tasks) {
  std::lock_guard<std::mutex> lk(m_queueMtx);
  m_totalCount.fetch_add(static_cast<int>(tasks.size()));
  for (const auto &t : tasks)
    m_queue.push_back(t);
  m_queueCv.notify_all();
}

void DownloadManager::start() {
  if (m_running.load())
    return;
  m_running = true;
  m_cancelled = false;
  m_completedCount = 0;
  m_failedCount = 0;

  for (int i = 0; i < m_workerCount; ++i)
    m_workers.emplace_back(&DownloadManager::workerLoop, this);

  // Qt Timer
  QMetaObject::invokeMethod(m_pollTimer.get(), "start");

  std::cout << "[INFO] İndirme Başlatıldı -> Worker Sayısı: " << m_workerCount
            << std::endl;
}

void DownloadManager::cancel() {
  m_cancelled = true;
  m_queueCv.notify_all();
  for (auto &w : m_workers)
    if (w.joinable())
      w.join();
  m_workers.clear();
  m_running = false;
  QMetaObject::invokeMethod(m_pollTimer.get(), "stop");
  std::cout << "[INFO] İndirmeler Dursuruldu." << std::endl;
}

void DownloadManager::waitUntilDone() {
  for (auto &w : m_workers)
    if (w.joinable())
      w.join();
  m_workers.clear();
  m_running = false;
  QMetaObject::invokeMethod(m_pollTimer.get(), "stop");
  pollProgress();
}

void DownloadManager::workerLoop() {
  while (true) {
    std::shared_ptr<DownloadTask> task;
    {
      std::unique_lock<std::mutex> lk(m_queueMtx);
      m_queueCv.wait(lk,
                     [&] { return !m_queue.empty() || m_cancelled.load(); });
      if (m_cancelled.load())
        return;
      if (m_queue.empty())
        return;
      task = m_queue.front();
      m_queue.pop_front();
    }

    // SHA-1 Check (Delta Update)
    if (!task->expectedSha1.empty() && fs::exists(task->destPath)) {
      if (verifySha1(task->destPath, task->expectedSha1)) {
        m_completedCount.fetch_add(1);
        std::cout << "[SKIP] Hash OK -> " << task->url << std::endl;
        continue;
      }
    }

    {
      std::lock_guard<std::mutex> lk(m_currentFileMtx);
      m_currentFile = fs::path(task->destPath).filename().string();
    }

    if (downloadOne(*task)) {
      m_completedCount.fetch_add(1);
    } else {
      m_failedCount.fetch_add(1);
    }

    // Check completion
    {
      std::lock_guard<std::mutex> lk(m_queueMtx);
      if (m_queue.empty())
        m_queueCv.notify_all();
    }
  }
}

bool DownloadManager::downloadOne(const DownloadTask &task) {
  try {
    fs::create_directories(fs::path(task.destPath).parent_path());

    // Log
    std::cout << "[INDIR] " << task.url << " -> " << task.destPath << std::endl;

    cpr::Response r =
        cpr::Get(cpr::Url{task.url}, cpr::Timeout{30000},
                 cpr::Header{{"User-Agent", "MixLauncher/2.0 (Linux)"}},
                 cpr::VerifySsl{false} // Linux SSL fix
        );

    if (r.status_code != 200) {
      std::cerr << "[HATA] HTTP " << r.status_code << " -> " << task.url
                << std::endl;
      return false;
    }

    // Write Buffer to File
    std::ofstream ofs(task.destPath, std::ios::binary);
    if (!ofs)
      return false;
    ofs.write(r.text.data(), static_cast<std::streamsize>(r.text.size()));
    ofs.close();

    // Verify SHA-1
    if (!task.expectedSha1.empty()) {
      if (!verifySha1(task.destPath, task.expectedSha1)) {
        std::cerr << "[HASH HATA] " << task.destPath << " -> Hash Tutmadi!"
                  << std::endl;
        fs::remove(task.destPath);
        return false;
      }
    }
    return true;
  } catch (const std::exception &e) {
    std::cerr << "[EXCEPTION] " << e.what() << " -> " << task.url << std::endl;
    return false;
  }
}

std::string DownloadManager::computeSha1(const std::string &filePath) {
  std::ifstream ifs(filePath, std::ios::binary);
  if (!ifs)
    return {};
  EVP_MD_CTX *ctx = EVP_MD_CTX_new();
  EVP_DigestInit_ex(ctx, EVP_sha1(), nullptr);
  char buf[16384];
  while (ifs.read(buf, sizeof(buf)))
    EVP_DigestUpdate(ctx, buf, ifs.gcount());
  if (ifs.gcount() > 0)
    EVP_DigestUpdate(ctx, buf, ifs.gcount());
  unsigned char hash[EVP_MAX_MD_SIZE];
  unsigned int hlen = 0;
  EVP_DigestFinal_ex(ctx, hash, &hlen);
  EVP_MD_CTX_free(ctx);
  std::ostringstream ss;
  for (unsigned int i = 0; i < hlen; ++i)
    ss << std::hex << std::setfill('0') << std::setw(2) << (int)hash[i];
  return ss.str();
}

bool DownloadManager::verifySha1(const std::string &filePath,
                                 const std::string &expected) {
  if (expected.empty())
    return true;
  return computeSha1(filePath) == expected;
}

void DownloadManager::pollProgress() {
  int done = m_completedCount.load();
  int fail = m_failedCount.load();
  int total = m_totalCount.load();

  QString cur;
  {
    std::lock_guard<std::mutex> lk(m_currentFileMtx);
    cur = QString::fromStdString(m_currentFile);
  }

  emit progressUpdated(done + fail, total, cur);

  if (total > 0 && (done + fail) >= total) {
    if (m_running.load()) {
      m_running = false;
      m_pollTimer->stop();
      // Wake up workers to exit
      m_cancelled = true;
      m_queueCv.notify_all();
      // Wait for workers
      for (auto &w : m_workers)
        if (w.joinable())
          w.join();
      m_workers.clear();
      emit allFinished(done, fail);
      std::cout << "[INFO] Tüm indirmeler tamamlandı: " << done << " Basarili, "
                << fail << " Hata." << std::endl;
    }
  }
}
