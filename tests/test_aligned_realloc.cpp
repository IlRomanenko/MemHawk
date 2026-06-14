#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <iostream>
#include <malloc.h>

size_t GetAlignment(uintptr_t value)
{
    return value & ((~value) + 1);
}

void Describe(void* ptr)
{
    auto ptrValue = reinterpret_cast<uintptr_t>(ptr);
    std::cout << "Usable: " << malloc_usable_size(ptr) << std::endl;
    std::cout << "Effective alignment: " << GetAlignment(ptrValue) << std::endl;
}

int main()
{
    constexpr size_t Size = 728;
    constexpr size_t Alignment = 1024;
    constexpr uint8_t Expected = 0xAB;

    void* ptr = aligned_alloc(Alignment, Size);
    memset(ptr, Expected, Size);

    Describe(ptr);

    void* nptr = realloc(ptr, Size + 19); // NOLINT(cppcoreguidelines-no-malloc)

    std::cout << std::endl;
    std::cout << "After realloc" << std::endl;
    std::cout << std::endl;

    Describe(nptr);

    std::cout << std::endl;
    std::cout << "Check data correctness: ";
    const auto data = reinterpret_cast<const unsigned char*>(nptr);
    for (size_t i = 0; i < Size; i++)
    {
        if (data[i] != Expected)
        {
            std::cout << "Failed" << std::endl;
            exit(-1);
        }
    }
    std::cout << "Passed" << std::endl;

    return 0;
}
