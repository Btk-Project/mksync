#pragma once

// Import config
#include <mksync/config.hpp>

// Import std
#include <algorithm>
#include <ranges>
#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <functional>
#include <thread>
#include <mutex>
#include <format>
#include <print>
#include <filesystem>
#include <chrono>

// Import ilias
#include <ilias/platform.hpp>
#include <ilias/process.hpp>
#include <ilias/signal.hpp>
#include <ilias/task.hpp>
#include <ilias/sync.hpp>
#include <ilias/net.hpp>
#include <ilias/io.hpp>

// Import spdlog
#include <spdlog/spdlog.h>

// Import win32
#if defined(_WIN32)
    #include <windows.h>
#endif