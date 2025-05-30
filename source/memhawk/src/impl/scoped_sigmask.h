#pragma once

#include <csignal> // IWYU pragma: keep

namespace memhawk
{

class ScopedSignalBlocker
{
public:
    ScopedSignalBlocker()
    {
        sigset_t set;
        sigfillset(&set);
        Setmask(SIG_SETMASK, &set, &m_oldMask);
    }

    ~ScopedSignalBlocker()
    {
        Setmask(SIG_SETMASK, &m_oldMask, nullptr);
    }

    ScopedSignalBlocker(const ScopedSignalBlocker &) = delete;
    ScopedSignalBlocker &operator=(const ScopedSignalBlocker &) = delete;

private:
    static void Setmask(int how, sigset_t* set, sigset_t* old)
    {
        pthread_sigmask(how, set, old);
    }

private:
    sigset_t m_oldMask{};
};

} // namespace memhawk
