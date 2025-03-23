#pragma once

#include "impl/memhawk.h"

namespace memhawk
{

class GlobalStorage
{
public:
    ~GlobalStorage();

    static GlobalStorage* GetGlobalStorage();
    static void Construct();
    static void Destroy();

    MemHawk* GetMemHawk();
    void PostponedConstruct();

private:
    GlobalStorage();

private:
    static std::unique_ptr<GlobalStorage> m_storage;

    std::unique_ptr<MemHawk> m_memHawk;
};

} // namespace memhawk
