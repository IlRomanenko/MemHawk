#include <sys/cdefs.h>

#include <cstdio>
#include <cstdlib>
#include <link.h>
#include <string>
#include <thread>
#include <vector>

static int callback(struct dl_phdr_info* hdr, size_t, void* data)
{
    auto names = reinterpret_cast<std::vector<std::string>*>(data);
    names->push_back(std::string{hdr->dlpi_name});
    return 0;
}

void worker()
{
    std::vector<std::string> names;
    for (size_t i = 0; i < 100'000; i++)
    {
        dl_iterate_phdr(callback, &names);
        names.clear();
    }
}

int main()
{
    constexpr const auto ThreadsCount = 8;
    std::vector<std::thread> threads;

    printf("before threads creation\n");
    threads.reserve(ThreadsCount);
    for (size_t i = 0; i < ThreadsCount; i++)
    {
        threads.emplace_back(worker);
    }
    printf("after threads creation\n");
    for (auto& th : threads)
    {
        if (th.joinable())
        {
            th.join();
        }
    }
    printf("after join\n");
    return 0;
}
