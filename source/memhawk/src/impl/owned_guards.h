#pragma once

#include "spinlock.h"
#include "tag_type.h"

#include <atomic>
#include <functional>
#include <list>
#include <memory>
#include <mutex>

namespace memhawk
{

class OwnedGuards
{
    class CallbackState;
    struct State;

public:
    class CallbackGuard;

    OwnedGuards() : m_state(std::make_shared<State>())
    {
    }

    ~OwnedGuards()
    {
        Drain();
    }

    std::unique_ptr<CallbackGuard> Register(std::function<void(void)> callback)
    {
        auto callbackState = std::make_shared<CallbackState>(std::move(callback));
        {
            const std::scoped_lock lock(m_state->mt);
            m_state->callbacks.push_back(callbackState);
        }
        return std::make_unique<CallbackGuard>(GuardTag<OwnedGuards>{}, m_state, callbackState);
    }

    void Drain()
    {
        std::list<std::shared_ptr<CallbackState>> local;
        {
            const std::scoped_lock lock(m_state->mt);
            std::swap(local, m_state->callbacks);
        }
        for (const auto& callback : local)
        {
            callback->execute();
        }
    }

    // Necessary for handling fork
    void UnsafeLock()
    {
        m_state->mt.lock();
    }

    void UnsafeUnlock()
    {
        m_state->mt.unlock();
    }

public:
    class CallbackGuard
    {
    public:
        explicit CallbackGuard(GuardTag<OwnedGuards>, std::shared_ptr<State> state,
                               std::shared_ptr<CallbackState> callback)
            : m_state(std::move(state)), m_callback(std::move(callback))
        {
        }

        ~CallbackGuard()
        {
            {
                const std::scoped_lock lock(m_state->mt);
                m_state->callbacks.remove(m_callback);
            }
            m_callback->execute();
        }

        CallbackGuard(const CallbackGuard&) = delete;
        CallbackGuard& operator=(const CallbackGuard&) = delete;

        friend OwnedGuards;

    private:
        std::shared_ptr<State> m_state;
        std::shared_ptr<CallbackState> m_callback;
    };

private:
    struct State
    {
        SpinLock mt;
        std::list<std::shared_ptr<CallbackState>> callbacks;
    };

    class CallbackState
    {
    public:
        explicit CallbackState(std::function<void()> callback) : m_callback(std::move(callback))
        {
        }

        void execute() noexcept
        {
            if (m_executed.exchange(true))
            {
                // was already executed before
                return;
            }
            try
            {
                m_callback();
            }
            catch (...) // NOLINT(bugprone-empty-catch)
            {
            }
        }

    private:
        std::function<void()> m_callback;
        std::atomic<bool> m_executed{false};
    };

private:
    std::shared_ptr<State> m_state;
};

} // namespace memhawk
