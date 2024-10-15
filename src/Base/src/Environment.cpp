#include "mksync/Base/Environment.hpp"

#include <codecvt>
#include <locale>

MKS_BEGIN
MKS_BASE_BEGIN

char **convert_argc_argv(size_t argc, const wchar_t *const *wargv, std::string &args, std::vector<char *> &argvector)
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

MKS_BASE_END
MKS_END