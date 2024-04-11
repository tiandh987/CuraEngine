// Copyright (c) 2024 UltiMaker
// CuraEngine is released under the terms of the AGPLv3 or higher

#include <iostream> //To change the formatting of std::cerr.
#include <signal.h> //For floating point exceptions.
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
#include <sys/resource.h> //For setpriority.
#endif

#ifdef SENTRY_URL
#ifdef _WIN32
#if ! defined(NOMINMAX)
#define NOMINMAX
#endif
#if ! defined(WIN32_LEAN_AND_MEAN)
#define WIN32_LEAN_AND_MEAN
#endif
#endif
#include <filesystem>
#include <semver.hpp>
#include <sentry.h>
#include <string>

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/conversion.hpp>
#include <spdlog/details/os.h>

#include "utils/format/filesystem_path.h"
#endif
#include <cstdlib>

#include <spdlog/spdlog.h>

#include "Application.h"

namespace cura
{

// Signal handler for a "floating point exception", which can also be integer division by zero errors.
// // “浮点异常”的信号处理程序，也可能是整数除以零错误。
void signal_FPE(int n)
{
    (void)n;
    spdlog::error("Arithmetic exception.");
    exit(1);
}

} // namespace cura

int main(int argc, char** argv)
{
#if defined(__linux__) || (defined(__APPLE__) && defined(__MACH__))
    // Lower the process priority on linux and mac. On windows this is done on process creation from the GUI.
    setpriority(PRIO_PROCESS, 0, 10);
#endif

#ifndef DEBUG
    // Register the exception handling for arithmetic exceptions, this prevents the "something went wrong" dialog on windows to pop up on a division by zero.
    // 注册 算术异常的异常处理，这可以防止窗口上的“出现错误”对话框在被零除时弹出。
    signal(SIGFPE, cura::signal_FPE);
#endif

    // 设置 std::cerr 流的输出格式，使其能够以布尔值的形式输出。
    std::cerr << std::boolalpha;


// Want to set the sentry URL? Use '-c user.curaengine:sentry_url=<url> -o curaengine:enable_sentry=True' with conan install
#ifdef SENTRY_URL
    if (const auto use_sentry = spdlog::details::os::getenv("USE_SENTRY"); ! use_sentry.empty() && use_sentry == "1")
    {
        // Setup sentry error handling.
        sentry_options_t* options = sentry_options_new();
        sentry_options_set_dsn(options, std::string(SENTRY_URL).c_str());
        spdlog::info("Sentry url: {}", std::string(SENTRY_URL).c_str());
        // This is also the default-path. For further information and recommendations:
        // https://docs.sentry.io/platforms/native/configuration/options/#database-path
#if defined(__linux__)
        const auto config_path = std::filesystem::path(fmt::format("{}/.local/share/cura/.sentry-native", std::getenv("HOME")));
#elif defined(__APPLE__) && defined(__MACH__)
        const auto config_path = std::filesystem::path(fmt::format("{}/Library/Application Support/cura/.sentry-native", std::getenv("HOME")));
#elif defined(_WIN64)
        const auto config_path = std::filesystem::path(fmt::format("{}\\cura\\.sentry-native", std::getenv("APPDATA")));
#endif
        spdlog::info("Sentry config path: {}", config_path);
        sentry_options_set_database_path(options, std::filesystem::absolute(config_path).generic_string().c_str());
        constexpr std::string_view cura_engine_version{ CURA_ENGINE_VERSION };
        const auto version = semver::from_string(cura_engine_version.substr(0, cura_engine_version.find_first_of('+')));
        if (ranges::contains(cura_engine_version, '+') || version.prerelease_type == semver::prerelease::alpha)
        {
            // Not a production build
            sentry_options_set_environment(options, "development");
        }
        else
        {
            sentry_options_set_environment(options, "production");
        }

        // Set the actual CuraEngine version
        sentry_options_set_release(options, fmt::format("curaengine@{}", cura_engine_version).c_str());
        spdlog::info("Starting sentry");
        sentry_init(options);
    }
#endif

    cura::Application::getInstance().run(argc, argv);

#ifdef SENTRY_URL
    if (const auto use_sentry = spdlog::details::os::getenv("USE_SENTRY"); ! use_sentry.empty() && use_sentry == "1")
    {
        spdlog::info("Closing sentry");
        sentry_close();
    }
#endif

    return 0;
}
