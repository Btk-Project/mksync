#include "mksync/proto/proto.hpp"

#include <iostream>

MKS_BEGIN

uint32_t proto_unused()
{
    std::cout << "mksync proto:" << __FUNCTION__ << std::endl;
    return 0;
}

MKS_END