#include <sys/wait.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <list>
#include <unistd.h>

void worker()
{
    constexpr const size_t Size = 1UL << 22;
    std::list<int> dq;
    for (size_t i = 0; i < Size; i++)
    {
        dq.emplace_back(i);
    }
    for (size_t i = 0; i < Size; i++)
    {
        dq.pop_front();
    }
}

int main()
{
    const auto begin = std::chrono::system_clock::now();

    auto pid = fork();
    if (pid == -1)
    {
        perror("Failed to fork");
        exit(-1);
    }

    if (pid == 0)
    {
        // child
        worker();
        return 0;
    }

    int status{};
    const auto res = waitpid(pid, &status, 0);
    if (res == -1)
    {
        perror("Failed to waitpid");
        exit(-1);
    }

    const auto end = std::chrono::system_clock::now();

    std::cout << "Waited: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count() << "ms"
              << std::endl;

    return 0;
}
