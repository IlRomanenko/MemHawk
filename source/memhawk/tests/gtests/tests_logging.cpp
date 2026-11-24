#include "impl/logging/log_record.h"

#include <gmock/gmock.h>
#include <gtest/gtest-matchers.h>
#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <limits>
#include <source_location>

namespace memhawk
{
using namespace logging;

std::source_location GetLocationTest()
{
    return std::source_location::current();
}

TEST(Logging, String_ExpectOk)
{
    logging::LogRecord record;
    record.Append("some large string");
    record.Append(std::string(":another string"));
    record.Append(std::string_view{":also a string"});

    EXPECT_EQ("some large string:another string:also a string\n", record.Data());
}

TEST(Logging, Boolean_ExpectOk)
{
    logging::LogRecord record;
    record.Append(true);
    record.Append(":");
    record.Append(false);

    EXPECT_EQ("true:false\n", record.Data());
}

TEST(Logging, Integral_ExpectOk)
{
    logging::LogRecord record;
    record.Append(10);
    record.Append(":");
    record.Append(42ULL);
    record.Append(":");
    record.Append(-123LL);

    EXPECT_EQ("10:42:-123\n", record.Data());
}

TEST(Logging, Integral_MaxValue_ExpectOk)
{
    logging::LogRecord record;
    record.Append(std::numeric_limits<uint8_t>::max());
    record.Append(":");
    record.Append(std::numeric_limits<uint16_t>::max());
    record.Append(":");
    record.Append(std::numeric_limits<uint32_t>::max());
    record.Append(":");
    record.Append(std::numeric_limits<uint64_t>::max());

    EXPECT_EQ("255:65535:4294967295:18446744073709551615\n", record.Data());
}

TEST(Logging, Integral_MinValue_ExpectOk)
{
    logging::LogRecord record;
    record.Append(std::numeric_limits<int8_t>::min());
    record.Append(":");
    record.Append(std::numeric_limits<int16_t>::min());
    record.Append(":");
    record.Append(std::numeric_limits<int32_t>::min());
    record.Append(":");
    record.Append(std::numeric_limits<int64_t>::min());

    EXPECT_EQ("-128:-32768:-2147483648:-9223372036854775808\n", record.Data());
}

TEST(Logging, Float_ExpectOk)
{
    logging::LogRecord record;
    record.Append(0.13456F);
    record.Append(":");
    record.Append(std::numeric_limits<float>::min());
    record.Append(":");
    record.Append(std::numeric_limits<float>::max());
    record.Append(":");
    record.Append(std::numeric_limits<float>::quiet_NaN());

    EXPECT_EQ("0.135:0.000:340282346638528859811704183484516925440.000:nan\n", record.Data());
}

TEST(Logging, Pointer_ExpectOk)
{
    const auto Pointer = reinterpret_cast<void*>(0xDEADBEEFDEADBEEF);
    logging::LogRecord record;
    record.Append(Pointer);

    EXPECT_EQ("0xdeadbeefdeadbeef\n", record.Data());
}

TEST(Logging, Hex_ExpectOk)
{
    logging::LogRecord record;
    record.Append(logging::Hex(0xDEADBEEFDEADBEEF));

    EXPECT_EQ("0xdeadbeefdeadbeef\n", record.Data());
}

TEST(Logging, HexNegative_ExpectOk)
{
    logging::LogRecord record;
    const int value = -0x10ABCDEF;
    auto formatter = logging::Hex(value);
    record.Append(formatter);

    EXPECT_EQ("-0x10abcdef\n", record.Data());
}

TEST(Logging, Binary_ExpectOk)
{
    logging::LogRecord record;
    uint16_t value = 0b1010001;
    record.Append(logging::Binary(value));
    record.Append(":");
    int16_t negValue = -0b1010001;
    record.Append(logging::Binary(negValue));

    EXPECT_EQ("0b1010001:-0b1010001\n", record.Data());
}

TEST(Logging, BinaryPositiveMax_ExpectOk)
{
    logging::LogRecord record;
    record.Append(logging::Binary(std::numeric_limits<int8_t>::max()));
    record.Append(":");
    record.Append(logging::Binary(std::numeric_limits<int16_t>::max()));
    record.Append(":");
    record.Append(logging::Binary(std::numeric_limits<int32_t>::max()));
    record.Append(":");
    record.Append(logging::Binary(std::numeric_limits<int64_t>::max()));

    EXPECT_EQ("0b1111111:0b111111111111111:0b1111111111111111111111111111111:"
              "0b111111111111111111111111111111111111111111111111111111111111111\n",
              record.Data());
}

TEST(Logging, BinaryNegativeMin_ExpectOk)
{
    logging::LogRecord record;
    record.Append(logging::Binary(std::numeric_limits<int8_t>::min()));
    record.Append(":");
    record.Append(logging::Binary(std::numeric_limits<int16_t>::min()));
    record.Append(":");
    record.Append(logging::Binary(std::numeric_limits<int32_t>::min()));
    record.Append(":");
    record.Append(logging::Binary(std::numeric_limits<int64_t>::min()));

    EXPECT_EQ("-0b10000000:-0b1000000000000000:-0b10000000000000000000000000000000:"
              "-0b1000000000000000000000000000000000000000000000000000000000000000\n",
              record.Data());
}

TEST(Logging, Timestamp_ExpectOk)
{
    constexpr auto Now = std::chrono::system_clock::time_point{std::chrono::nanoseconds{1795541215123956467}};
    logging::LogRecord record;
    const auto timestamp = logging::FormatUtcTimestamp(Now);
    ASSERT_TRUE(timestamp);
    const auto timestampStr = std::string(timestamp->data());
    EXPECT_EQ("2026-11-24T17:26:55.123Z", timestampStr);
    record.Append(*timestamp);

    EXPECT_EQ("2026-11-24T17:26:55.123Z\n", record.Data());
}

TEST(Logging, SourceLocation_ExpectOk)
{
    logging::LogRecord record;
    record.Append(GetLocationTest());

    EXPECT_EQ("std::source_location memhawk::GetLocationTest():18\n", record.Data());
}

} // namespace memhawk
