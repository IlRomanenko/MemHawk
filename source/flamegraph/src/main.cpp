#include "graph.h"

#include <cstddef>
#include <cstdint>
#include <iostream>
#include <string>
#include <system_error>

#define CHECK(x)                                                                                                       \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(x)) /* NOLINT(readability-simplify-boolean-expr) */                                                      \
        {                                                                                                              \
            return false;                                                                                              \
        }                                                                                                              \
    } while (0)

struct LineValue
{
    int64_t value{};
    std::vector<uint32_t> path;
};

template <typename T>
bool parse_number(const char*& begin, const char* end, T& value)
{
    auto res = std::from_chars(begin, end, value);
    CHECK(res.ptr != begin);
    begin = res.ptr;
    return res.ec == std::errc();
}

template <typename T>
bool parse_number(std::string_view str, T& value)
{
    const char* begin = str.data();
    const char* end = str.data() + str.size();
    bool res = parse_number(begin, end, value);
    return res && begin == end;
}

bool parse_line(std::string_view str, LineValue& data)
{
    data.value = 0;
    data.path.clear();
    if (str.empty())
    {
        return true;
    }

    const char* ptr = str.begin();
    const char* end = ptr + str.size();

    int64_t value{};

    CHECK(parse_number(ptr, end, value));
    data.value = value;

    CHECK(ptr < end && *ptr == '\t');
    ptr++;
    CHECK(ptr < end && *ptr == '[');
    ptr++;

    while (ptr < end && *ptr != ']')
    {
        uint32_t val{};
        CHECK(parse_number(ptr, end, val));
        data.path.push_back(val);
        CHECK(ptr < end && (*ptr == ',' || *ptr == ']'));
        ptr++;
    }
    return true;
}

void process(size_t count, bool stats)
{
    std::string buf;
    buf.reserve(4096);

    graph::CompressedTrie graph;
    LineValue line{};

    while (std::getline(std::cin, buf))
    {
        if (!parse_line(buf, line))
        {
            std::cerr << "Incorrect line: " << buf << "\n";
            return;
        }
        if (line.path.size() < 1 || line.path.front() != 0)
        {
            std::cerr << "Missed root node: " << buf << "\n";
            return;
        }
        // path without root node
        const auto path = std::span{line.path}.subspan(1);
        graph.Append(path, line.value);
    }

    if (stats)
    {
        graph.Stats();
        return;
    }
    const auto result = graph.GetTop(count);

    for (size_t ind = 0; ind < result.size(); ind++)
    {
        const auto& elem = result[ind];
        std::cout << ind << '\t' << elem.key << '\t' << elem.depth << '\t' << elem.total << '\t' << elem.value << '\n';
    }
}

void help()
{
    // clang-format off
    std::cout << "Usage: ./flamegraph <arg>" << "\n"
              << "stats -> print stats only" << "\n"
              << "<count> -> non negative integer, print top <count> nodes for flamegraph" << "\n";
    // clang-format on
}

int main(int argc, char** argv)
{
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    size_t count = 1000;
    bool stats = false;

    if (argc == 2)
    {
        std::string arg{argv[1]};
        if (arg == "stats")
        {
            stats = true;
        }
        else
        {
            if (!parse_number(arg, count))
            {
                help();
                exit(-1);
            }
        }
    }
    else if (argc != 1)
    {
        help();
        exit(-1);
    }

    process(count, stats);

    std::cout.flush();
    return 0;
}
