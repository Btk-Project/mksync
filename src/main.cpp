#include <ilias/platform.hpp>

auto main(int argc, char** argv) -> int {
    ilias::PlatformContext ctxt;
    ctxt.install();

    return 0;
}