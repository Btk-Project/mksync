#include "mksync/base/osdep.h"

#include <iostream>
#include <functional>
#include <string>

#include <cxxopts.hpp>
#include <ilias/platform.hpp>
#include <ilias/fs/console.hpp>
#include <ilias/task.hpp>
#include <ilias/sync/scope.hpp>

#include "mksync/base/environment.hpp"
#include "mksync/proto/proto.hpp"

using ILIAS_NAMESPACE::Console;
using ILIAS_NAMESPACE::Error;
using ILIAS_NAMESPACE::PlatformContext;
using ILIAS_NAMESPACE::Result;
using ILIAS_NAMESPACE::SystemError;
using ILIAS_NAMESPACE::Task;
using ILIAS_NAMESPACE::TaskScope;

#define argsparse(argc, argv)                                                                      \
    (void)argc;                                                                                    \
    (void)argv;

struct Context {
    bool                                                                mIsExit = false;
    std::map<std::string, std::function<Task<void>(Context &, void *)>> mCommands;
};

Task<void> task1(Context &ctxt, void *args)
{
    (void)args;
    (void)ctxt;
    std::cout << "run task1" << std::endl;
    co_return {};
}

Task<void> main_loop(Context &ctx)
{
    std::cout << "main loop" << std::endl;
    auto ret = Console::fromStdin(*PlatformContext::currentThread());
    if (!ret) {
        std::cout << "read error: " << ret.error().toString() << std::endl;
        co_return Result<void>();
    }
    Console console = std::move(ret.value()); // get stdin console

    cxxopts::Options opts("backend", "mksync's backend");
    opts.add_options()("run", "running")("exit", "exit program")("command", "commands");

    ctx.mCommands["task1"] = std::bind(task1, std::placeholders::_1,
                                       std::placeholders::_2); // bind a task to a command

    std::string readBuffer(1024, '\0');
    TaskScope   scop; // create a task scope, which will be canceled when the
                      // scope is out of scope
    scop.setAutoCancel(true);

    while (!ctx.mIsExit) {
        auto ret =
            co_await console.read(ilias::makeBuffer(readBuffer)); // TODO: read command from stdin
        if (!ret) {
            // TODO: error handling
            std::cout << "read error: " << ret.error().toString() << std::endl;
            co_return Result<void>();
        }
        std::cout << "read " << ret.value() << std::endl;
        // TODO: process input
        int                       argc;
        std::vector<const char *> argvector;
        std::string               args = mks::base::string_to_argc_argv(
            std::string(readBuffer.begin(), readBuffer.begin() + ret.value()), argc, argvector);

        auto optres = opts.parse(argc, argvector.data());

        for (auto &arg : argvector) {
            std::cout << "p:" << (void *)arg << "arg:" << arg << "*\n";
        }

        if (optres.count("run") != 0) {
            std::cout << "run" << std::endl;
        }
        else if (optres.count("exit") != 0) {
            // TODO: exit main loop
            ctx.mIsExit = true;
            std::cout << "exit" << std::endl;
        }
        else if (optres.count("command") != 0) {
            // TODO: config main loop
            scop.spawn(std::bind(task1, ctx, nullptr));
            std::cout << "command" << std::endl;
        }
        else {
            std::cout << opts.help() << std::endl;
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

    argsparse(argc, argv); // parse arguments
    // TODO: initialize services, read arguments, etc.

    PlatformContext context;
    Context         ctx;
    ilias_wait      main_loop(ctx); // start main loop

    return 0;
}