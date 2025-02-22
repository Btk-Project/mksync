#include <ilias/ilias.hpp>
#include <ilias/net/tcp.hpp>
#include <ilias/fs/console.hpp>
#include <nekoproto/communication/communication_base.hpp>
#include <spdlog/spdlog.h>

#include "app.hpp"
#include "win_context.hpp"

int main(int argc, const char *const *argv)
{
    ILIAS_LOG_SET_LEVEL(ILIAS_INFO_LEVEL);
    spdlog::set_level(spdlog::level::debug);
    WinContext context;
    App        app;
    ilias_wait app.exec(argc, argv);
    return 0;
}