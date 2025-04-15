#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <nekoproto/communication/communication_base.hpp>
#include <spdlog/spdlog.h>

#include "mksync/core/app.hpp"
#include "mksync/core/io_context.hpp"

int main(int argc, const char *const *argv)
{

#if defined(_WIN32)
    ::SetConsoleCP(65001);
    ::SetConsoleOutputCP(65001);
    std::setlocale(LC_ALL, ".utf-8");
#endif

    ILIAS_LOG_SET_LEVEL(ILIAS_INFO_LEVEL);
    spdlog::set_level(spdlog::level::warn);
    spdlog::set_pattern("%^[%l] [%Y-%m-%d %H:%M:%S.%e] [%s:%#]%$ %v");
    mks::IoContext context;
    mks::App       app(&context);
    ilias_wait     app.exec(argc, argv);
    return 0;
}