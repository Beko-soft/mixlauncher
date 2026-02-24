#include "MainWindow.h"
#include <QApplication>
#include <QFont>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

int main(int argc, char *argv[]) {
  // ── Logging ──────────────────────────────────────────
  try {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        "mixlauncher.log", true);
    auto logger = std::make_shared<spdlog::logger>(
        "mc", spdlog::sinks_init_list{console_sink, file_sink});
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);
    spdlog::info("MixLauncher v2.0 baslatiliyor...");
  } catch (const spdlog::spdlog_ex &ex) {
    // Fallback – console only
  }

  // ── Qt Application ───────────────────────────────────
  QApplication app(argc, argv);
  app.setApplicationName("MixLauncher");
  app.setApplicationVersion("2.0.0");

  QFont defaultFont("Segoe UI", 10);
  defaultFont.setStyleStrategy(QFont::PreferAntialias);
  app.setFont(defaultFont);

  MainWindow w;
  w.resize(960, 640);
  w.show();

  return app.exec();
}
