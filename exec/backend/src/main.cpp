#include "mksync/Base/osdep.h"

#include <iostream>

#include <ilias/platform.hpp>
#include <ilias/fs/console.hpp>
#include <ilias/task.hpp>
#include <ilias/sync/scope.hpp>
#include <functional>
#include <span>
#include <string>
#include <iostream>

#include "mksync/Base/Environment.hpp"
#include "mksync/Proto/Proto.hpp"


using ILIAS_NAMESPACE::Task;
using ILIAS_NAMESPACE::PlatformContext;
using ILIAS_NAMESPACE::Console;
using ILIAS_NAMESPACE::Result;
using ILIAS_NAMESPACE::Error;
using ILIAS_NAMESPACE::SystemError;
using ILIAS_NAMESPACE::TaskScope;

#define argsparse(argc, argv) (void)argc; (void)argv;

enum {
    COMMAND = 0,
    EXIT = 1,
    CONFIG = 2,
};

struct Option {
    int type;
    std::string name;
    void* args; // do something, or setting some value
};


Option inputParse(std::span<std::byte> arg) {
    if (std::string_view(reinterpret_cast<char*>(arg.data()), arg.size() - 1) == "exit") {
        return Option{EXIT, "", nullptr};
    } else if (std::string_view(reinterpret_cast<char*>(arg.data()), arg.size() - 1) == "run") {
        return Option{COMMAND, "task1", nullptr};
    }
    return Option{CONFIG, "", nullptr};
}

struct Context {
bool mIsExit = false;
std::map<std::string, std::function<Task<void>(Context&, void*)>> mCommands;
};

Task<void> Task1(Context& ctxt, void* args) {
    (void)args;
    (void)ctxt;
    std::cout << "run task1" << std::endl;
    co_return {};
}

Task<void> MainLoop(Context& ctx) {
    std::cout << "main loop" << std::endl;
    auto ret = Console::fromStdin(*PlatformContext::currentThread());
    if (!ret) {
        std::cout << "read error: " << ret.error().toString() << std::endl;
        co_return Result<void>();
    }
    Console console = std::move(ret.value()); // get stdin console

    ctx.mCommands["task1"] = std::bind(Task1, std::placeholders::_1, std::placeholders::_2); // bind a task to a command

    std::vector<std::byte> readBuffer;
    readBuffer.resize(1024);
    TaskScope scop; // create a task scope, which will be canceled when the scope is out of scope
    scop.setAutoCancel(true);

    while (!ctx.mIsExit) {
        auto ret = co_await console.read(readBuffer); // TODO: read command from stdin
        if (!ret) {
            // TODO: error handling
            std::cout << "read error: " << ret.error().toString() << std::endl;
            co_return Result<void>();
        }
        std::cout << "read " << ret.value() << std::endl;
        // TODO: process input
        auto option = inputParse({readBuffer.data(), ret.value()});
        switch (option.type) {
            case COMMAND:
                // create task and suspend it
                scop.spawn(ctx.mCommands[option.name](ctx, option.args));
                std::cout << "command: " << option.name << std::endl;
                break;
            case CONFIG:
                // TODO: config main loop
                std::cout << "config" << std::endl;
                break;
            case EXIT:
                // TODO: exit main loop
                ctx.mIsExit = true;
                std::cout << "exit" << std::endl;
                break;
            default:
                break;
        }
    }
    // TODO: cancel and wait for all tasks to finish
    scop.cancel();
    scop.wait();
    co_return {};
}

#ifdef _WIN32
int wmain([[maybe_unused]] int argc, [[maybe_unused]] wchar_t *wargv[])
#else
int main([[maybe_unused]] int argc, [[maybe_unused]] char *argv[])
#endif
{
    // utf-8 encoding
    std::setlocale(LC_ALL, ".utf-8");
#ifdef _WIN32
    std::string             args      = {};
    std::vector<char *>     argvector = {};
    [[maybe_unused]] char **argv      = mks::base::convert_argc_argv(argc, wargv, args, argvector);
#endif

    // ilias::Console console = {};

    argsparse(argc, argv); // parse arguments
    // TODO: initialize services, read arguments, etc.

    PlatformContext context;
    Context ctx;
    ilias_wait MainLoop(ctx); // start main loop

    return 0;
}