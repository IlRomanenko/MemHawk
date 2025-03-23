#include <atomic>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <list>
#include <thread>

std::atomic_bool gl_start = {false};
std::atomic_uint32_t gl_finishedWorkers = 0;
std::atomic_bool gl_finished = {false};

uint32_t gl_totalWorkers = 0;

void worker()
{
    gl_start.wait(false);
    constexpr const size_t Size = 1UL << 22;
    std::list<int> dq;
    const auto begin = std::chrono::steady_clock::now();
    for (auto i = 0; i < Size; i++)
    {
        dq.emplace_back(i);
    }
    for (auto i = 0; i < Size; i++)
    {
        dq.pop_front();
    }
    gl_finishedWorkers++;
    if (gl_finishedWorkers == gl_totalWorkers)
    {
        gl_finished = true;
        gl_finished.notify_all();
    }
}

int main()
{
    std::vector<std::thread> workers;
    gl_totalWorkers = std::thread::hardware_concurrency();
    workers.reserve(gl_totalWorkers);
    for (size_t i = 0; i < gl_totalWorkers; i++)
    {
        workers.emplace_back(std::thread([]() { worker(); }));
    }

    const auto begin = std::chrono::steady_clock::now();
    gl_start = true;
    gl_start.notify_all();

    gl_finished.wait(false);
    const auto end = std::chrono::steady_clock::now();

    std::cout << "Workers: " << gl_totalWorkers
              << " Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms"
              << std::endl;

    for (auto& thread : workers)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }


    return 0;
}
