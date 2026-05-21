#include <list>
#include <thread>
#include <vector>

void worker()
{
    std::list<size_t> data;
    for (size_t i = 0; i < 100'000; i++)
    {
        data.push_back(i);
    }
    data.clear();
}

int main()
{
    constexpr const auto ThreadsCount = 8;
    printf("before threads creation\n");
    std::vector<std::thread> threads;
    threads.reserve(ThreadsCount);
    for (size_t i = 0; i < ThreadsCount; i++)
    {
        threads.emplace_back(worker);
        threads.back().detach();
    }
    printf("main finishes\n");
    return 0;
}
