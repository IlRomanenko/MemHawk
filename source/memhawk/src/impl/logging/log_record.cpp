#include "log_record.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <ctime>
#include <optional>
#include <unistd.h>

namespace memhawk::logging
{

std::optional<UtcTimestamp> FormatUtcTimestamp(std::chrono::system_clock::time_point now) noexcept
{
    using namespace std::chrono;
    const sys_seconds instant{floor<seconds>(now.time_since_epoch())};
    const auto midnight = floor<days>(instant);
    constexpr sys_days FirstDay{year{0} / January / 1};
    constexpr sys_days LastDay{year{9999} / December / 31};
    if (midnight < FirstDay || midnight > LastDay)
    {
        return std::nullopt;
    }

    const year_month_day date{midnight};
    const hh_mm_ss time{instant - midnight};
    const milliseconds msec = duration_cast<milliseconds>(now - instant);
    auto timestamp = UtcTimestampLayout;

    const auto digits = [&timestamp](size_t offset, size_t width, uint32_t value) {
        while (width != 0)
        {
            width--;
            timestamp[offset + width] = static_cast<char>('0' + value % 10);
            value /= 10;
        }
    };

    digits(0, 4, static_cast<uint32_t>(static_cast<int>(date.year())));
    digits(5, 2, static_cast<uint32_t>(date.month()));
    digits(8, 2, static_cast<uint32_t>(date.day()));
    digits(11, 2, static_cast<uint32_t>(time.hours().count()));
    digits(14, 2, static_cast<uint32_t>(time.minutes().count()));
    digits(17, 2, static_cast<uint32_t>(time.seconds().count()));
    digits(20, 3, static_cast<uint32_t>(msec.count()));
    return timestamp;
}

} // namespace memhawk::logging
