
#include "impl/memhawk.h"

#include <gtest/gtest.h>

#include <cstddef>

namespace memhawk
{

class MemHawkFixture : public testing::Test
{
public:
protected:
    std::unique_ptr<MemHawk> m_memhawk;
};

} // namespace memhawk
