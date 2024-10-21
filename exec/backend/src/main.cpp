#include "main.hpp"

#include <iostream>
#include <functional>
#include <string>

#include <cxxopts.hpp>
#include <ilias/platform.hpp>
#include <ilias/fs/console.hpp>
#include <ilias/task.hpp>
#include <ilias/sync/scope.hpp>
#include <ilias/task/when_any.hpp>
#include <spdlog/spdlog.h>
#include <so_5/all.hpp>

#include "mksync/base/environment.hpp"
#include "mksync/proto/proto.hpp"

#define argsparse(argc, argv)                                                                      \
    (void)argc;                                                                                    \
    (void)argv;

ilias::Task<void> task1(void *args)
{
    (void)args;
    std::cout << "run task1" << std::endl;
    co_return {};
}

class TestAgent : public so_5::agent_t {
public:
    TestAgent(so_5::agent_context_t ctx) : so_5::agent_t(ctx) {}
    ~TestAgent() = default;

    void so_define_agent() override;
    void so_evt_start() override;
    void so_evt_finish() override;
};

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

    // Ilias
    ilias::PlatformContext context;

    // so_5
    so_5::wrapped_env_t  sobj = {};
    so_5::environment_t &env  = sobj.environment();
    // work group and dispatcher
    auto coop = env.make_coop(); // main work group
    auto activeObjDisp =
        so_5::disp::active_obj::make_dispatcher(env); // dispatcher (each agent in a thread)
    // make work groups
    (void)coop->make_agent_with_binder<TestAgent>(activeObjDisp.binder());
    // register and start work groups
    env.register_coop(std::move(coop));

    // ilias main loop
    ilias_wait main_loop();

    // stop so_5
    sobj.stop_then_join();
    return 0;
}

ilias::Task<int> main_loop()
{
    auto [res1] = co_await ilias::whenAny(console_loop());
    if (res1 && *res1) {
        std::cout << "console loop end with: " << res1->value() << std::endl;
    }
    co_return {};
}

ilias::Task<int> console_loop()
{
    std::cout << "console loop" << std::endl;

    cxxopts::Options opts("backend", "mksync's backend");
    opts.add_options()("run", "running")("exit", "exit program")("command", "commands");

    ilias::Console console;
    {
        auto res = ilias::Console::fromStdin(*ilias::PlatformContext::currentThread());
        if (!res) {
            std::cout << "read error: " << res.error().toString() << std::endl;
            co_return ilias::Result<int>(res.error().value());
        }
        console = std::move(res.value()); // get stdin console
    }
    std::string      readBuffer(1024, '\0');
    ilias::TaskScope scop; // create a task scope, which will be canceled when out of scope
    scop.setAutoCancel(true);

    while (true) {
        size_t readBytes;
        {
            auto res = co_await console.read(ilias::makeBuffer(readBuffer));
            if (!res) {
                std::cout << "read error: " << res.error().toString() << std::endl;
                co_return ilias::Result<int>(res.error().value());
            }
            readBytes = std::move(res.value());
        }
        std::cout << "read bytes: " << readBytes << std::endl;

        // parse command
        int                       argc;
        std::vector<const char *> argvector;
        std::string               args = mks::base::string_to_argc_argv(
            std::string(readBuffer.begin(), readBuffer.begin() + readBytes), argc, argvector);

        auto optres = opts.parse(argc, argvector.data());

        for (auto &arg : argvector) {
            std::cout << "p:" << (void *)arg << "arg:" << arg << "*\n";
        }

        if (optres.count("run") != 0) {
            std::cout << "run" << std::endl;
        }
        else if (optres.count("exit") != 0) {
            std::cout << "exit" << std::endl;
            break;
        }
        else if (optres.count("command") != 0) {
            // TODO: config main loop
            scop.spawn(std::bind(task1, nullptr));
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

void TestAgent::so_define_agent()
{
    std::cout << __FUNCTION__ << std::endl;
}

void TestAgent::so_evt_start()
{
    std::cout << __FUNCTION__ << std::endl;
}
void TestAgent::so_evt_finish()
{
    std::cout << __FUNCTION__ << std::endl;
}