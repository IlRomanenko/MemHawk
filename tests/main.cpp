#include <sys/cdefs.h>

#include <cstdio>
#include <cstdlib>
#include <list>
#include <thread>

__attribute__((used)) void* foo()
{
    auto ptr = malloc(10); // NOLINT(cppcoreguidelines-no-malloc)
    return ptr;
}

__attribute__((used)) void* bar()
{
    auto ptr = foo();
    return ptr;
}

int main()
{
    constexpr size_t TotalAllocs = 1'000'000;
    std::list<int> ls;
    for (size_t i = 0; i < TotalAllocs; i++)
    {
        ls.emplace_back(i);
    }
    ls.clear();
    ls.resize(0);

    size_t x = 0;
    for (size_t i = 0; i < TotalAllocs; i++)
    {
        void* q = bar();
        x += reinterpret_cast<size_t>(q);
        free(q); // NOLINT(cppcoreguidelines-no-malloc)
    }
    printf("%zu\n", x);

    for (size_t i = 0; i < 42; i++)
    {
        std::thread th([]() {});
        th.join();
    }

    printf("end of main\n");
    return 0;
}
