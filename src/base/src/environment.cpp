#include "mksync/base/environment.hpp"

#include <codecvt>
#include <locale>
#include <sstream>
#include <iterator>

MKS_BEGIN

char **convert_argc_argv(size_t argc, const wchar_t *const *wargv, std::string &args,
                         std::vector<char *> &argvector)
{
    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter = {};
    argvector.resize(argc);

    std::vector<size_t> pos(argc + 1, args.size());
    for (size_t i = 0; i < argc; ++i) {
        args.append(converter.to_bytes(wargv[i])).append(1, '\0');
        pos[i + 1] = args.size();
    }
    for (size_t i = 0; i < argc; ++i) {
        argvector[i] = &args[pos[i]];
    }
    return argvector.data();
}

std::string string_to_argc_argv(const std::string &str, int &argc,
                                std::vector<const char *> &argvector)
{
    std::istringstream       iss(str);
    std::vector<std::string> args = {std::istream_iterator<std::string>{iss},
                                     std::istream_iterator<std::string>{}};

    argc = static_cast<int>(args.size());
    argvector.resize(argc);

    std::string buffer = {};
    for (auto &arg : args) {
        buffer.append(arg).append(1, '\0');
    }

    if (argc == 0) {
        return buffer;
    }

    argvector[0] = buffer.data();
    for (int i = 1; i < argc; ++i) {
        argvector[i] = argvector[i - 1] + args[i - 1].size() + 1;
    }
    return buffer;
}

MKS_END