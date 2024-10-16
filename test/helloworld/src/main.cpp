#include "mksync/base/osdep.h"

#include <iostream>
#include <codecvt>
#include <locale>

#include "mksync/proto/proto.hpp"

int main()
{
    std::cout << "Hello world\n";
    mks::proto_unused();

    std::wstring_convert<std::codecvt_utf8<wchar_t>, wchar_t> converter = {};

    return 0;
}