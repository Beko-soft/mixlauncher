cmake_minimum_required(VERSION 3.16)
project(MixLauncher VERSION 2.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Hızlandırma optimizasyonu (-O3, -flto)
set(CMAKE_CXX_FLAGS "-O3 -flto ${CMAKE_CXX_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "-flto ${CMAKE_EXE_LINKER_FLAGS}")

# Qt MOC / UIC / RCC
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# ── Dependencies ──────────────────────────────────────────
include(FetchContent)

# Qt6
find_package(Qt6 REQUIRED COMPONENTS Widgets Network Concurrent)

# OpenSSL
find_package(OpenSSL REQUIRED)

# nlohmann/json
FetchContent_Declare(json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(json)

# CPR
FetchContent_Declare(cpr
    URL https://github.com/libcpr/cpr/archive/refs/tags/1.10.2.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
set(CPR_USE_SYSTEM_CURL OFF CACHE BOOL "" FORCE)
set(CPR_BUILD_TESTS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(cpr)

# spdlog
FetchContent_Declare(spdlog
    URL https://github.com/gabime/spdlog/archive/refs/tags/v1.12.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
FetchContent_MakeAvailable(spdlog)

# ── Sources ───────────────────────────────────────────────
file(GLOB_RECURSE SOURCES "src/*.cpp" "src/*.h")

add_executable(MixLauncher ${SOURCES})

target_include_directories(MixLauncher PRIVATE src)

target_link_libraries(MixLauncher PRIVATE
    Qt6::Widgets Qt6::Network Qt6::Concurrent
    nlohmann_json::nlohmann_json
    cpr::cpr
    spdlog::spdlog
    OpenSSL::SSL OpenSSL::Crypto
    pthread
)

# ── CPack Installer Generation ────────────────────────────
include(InstallRequiredSystemLibraries)

install(TARGETS MixLauncher DESTINATION bin)
install(FILES LICENSE DESTINATION share/doc/MixLauncher)

set(CPACK_PACKAGE_NAME "MixLauncher")
set(CPACK_PACKAGE_VENDOR "MixLauncher Team")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Professional Minecraft Launcher")
set(CPACK_PACKAGE_VERSION "2.0.0")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "MixLauncher")

# Windows (NSIS)
set(CPACK_GENERATOR "NSIS;DEB;TGZ")
set(CPACK_NSIS_DISPLAY_NAME "MixLauncher 2.0")
set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/assets/icon.ico") 
set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/assets/icon.ico")
set(CPACK_NSIS_MODIFY_PATH ON)

# Linux (DEB)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "berkay@example.com")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

include(CPack)
