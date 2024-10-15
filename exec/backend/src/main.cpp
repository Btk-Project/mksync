#include "mksync/Base/osdep.h"

#include <iostream>

#include <ilias/platform.hpp>
#include <ilias/fs/console.hpp>

#include "mksync/Base/Environment.hpp"
#include "mksync/Proto/Proto.hpp"

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
    
    while (true) {
        // auto ret = co_await readFromUser();
    }
    return 0;
}