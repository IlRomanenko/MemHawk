#pragma once

#include "memhawk.h"

class GlobalStorage
{
public:
    ~GlobalStorage();

    MemHawk* GetMemHawk();

    static GlobalStorage* GetGlobalStorage();
    static void Construct();

private:
    GlobalStorage();

private:
    static std::unique_ptr<GlobalStorage> m_global;
    std::unique_ptr<MemHawk> m_memHawk;
};
