#include "mksync/display/display.hpp"

#include <iostream>

MKS_BEGIN
MKS_DISPLAY_BEGIN

uint32_t display_unused()
{
    std::cout << "mksync display:" << __FUNCTION__ << std::endl;
    return 0;
}

MKS_END
MKS_DISPLAY_END