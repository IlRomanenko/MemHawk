
#include "global_storage.h"

#include "impl/log.h"
#include "impl/memhawk.h"

#include <absl/base/attributes.h>

#include <memory>

namespace memhawk
{

ABSL_CONST_INIT bool gl_storageReady = false;
ABSL_CONST_INIT std::unique_ptr<GlobalStorage> GlobalStorage::m_storage = {};

void GlobalStorage::Construct()
{
    m_storage = std::unique_ptr<GlobalStorage>(new GlobalStorage());
    gl_storageReady = true;
}

void GlobalStorage::Destroy()
{
    gl_storageReady = false;
    m_storage.reset();
}

GlobalStorage* GlobalStorage::GetGlobalStorage()
{
    if (gl_storageReady)
    {
        return m_storage.get();
    }
    return nullptr;
}

void GlobalStorage::PostponedConstruct()
{
    LogDebug("begin");
    m_memHawk->PostponedConstruct();
    LogDebug("end");
}

GlobalStorage::GlobalStorage()
{
    LogDebug("begin");
    m_memHawk = std::make_unique<MemHawk>();
    LogDebug("end");
}

GlobalStorage::~GlobalStorage()
{
    LogDebug("begin");
    gl_storageReady = false;
    m_memHawk.reset();
    LogDebug("end");
}

MemHawk* GlobalStorage::GetMemHawk()
{
    return m_memHawk.get();
}

} // namespace memhawk
