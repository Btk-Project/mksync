#include "mksync/Proto/Proto.hpp"

#include <iostream>

namespace mks
{
    uint32_t proto_unused()
    {
        std::cout << "mksync proto:" << __FUNCTION__ << std::endl;
        return 0;
    }
} // namespace mks