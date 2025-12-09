#pragma once

#include <atomic>
#include <emmintrin.h>

namespace memhawk
{
class SpinLock
{
public:
    void lock()
    {
        while (std::atomic_flag_test_and_set_explicit(&m_lock, std::memory_order_acquire))
        {
            _mm_pause();
        }
    }

    void unlock()
    {
        std::atomic_flag_clear_explicit(&m_lock, std::memory_order_release);
    }

private:
    std::atomic_flag m_lock = ATOMIC_FLAG_INIT;
};
} // namespace memhawk
