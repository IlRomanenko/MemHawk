
#include "global_storage.h"

#include "log.h"
#include "memhawk.h"

LogLevel gl_logLevel = LogLevel::Info;
int gl_logFile = STDERR_FILENO;

static bool glInitialised = false;
std::unique_ptr<GlobalStorage> GlobalStorage::m_global = {};

struct OnConstruct
{
    OnConstruct()
    {
        Stacktrace::Setup();
        GlobalStorage::Construct();
    }

    ~OnConstruct()
    {
    }
};

static OnConstruct glOnConstruct{};

void GlobalStorage::Construct()
{
    LogDebug("begin");
    m_global = std::unique_ptr<GlobalStorage>(new GlobalStorage());
    m_global->GetMemHawk()->PostponedConstruct();
    LogDebug("end");
}

GlobalStorage* GlobalStorage::GetGlobalStorage()
{
    if (glInitialised) {
        return m_global.get();
    }
    return nullptr;
}

GlobalStorage::GlobalStorage()
{
    LogDebug("begin");
    m_memHawk = std::make_unique<MemHawk>();
    glInitialised = true;
    LogDebug("end");
}

GlobalStorage::~GlobalStorage()
{
    LogDebug("begin");
    glInitialised = false;
    m_memHawk.reset();
    LogDebug("end");
}

MemHawk* GlobalStorage::GetMemHawk()
{
    return m_memHawk.get();
}
