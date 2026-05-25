#include <ilias/platform.hpp>
#include <stacktrace>
#include <print>

void crashHandler() {
    std::println("Crashed");
    std::println("Stacktrace:");
    std::println("{}", std::stacktrace::current());
}

void ilias_main() {
    co_return;
}