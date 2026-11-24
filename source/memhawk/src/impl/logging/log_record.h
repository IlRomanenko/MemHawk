#pragma once

#include <absl/base/attributes.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace memhawk::logging
{

constexpr auto UtcTimestampLayout = std::to_array("0000-00-00T00:00:00.000Z");
using UtcTimestamp = std::array<char, UtcTimestampLayout.size()>;
std::optional<UtcTimestamp> FormatUtcTimestamp(std::chrono::system_clock::time_point now) noexcept;

template <std::integral Integer>
struct Hex
{
    using Unsigned = std::make_unsigned_t<Integer>;

    explicit Hex(Integer value) : m_negative{value < 0}, m_value{static_cast<Unsigned>(value)}
    {
        if (m_negative)
        {
            m_value = (~m_value) + 1;
        }
    }

    const Unsigned& Data() const noexcept
    {
        return m_value;
    }

    bool Negative() const noexcept
    {
        return m_negative;
    }

private:
    bool m_negative{};
    Unsigned m_value{};
};

template <std::integral Integer>
struct Binary
{
    using Unsigned = std::make_unsigned_t<Integer>;

    explicit Binary(Integer value) : m_negative{value < 0}, m_value{static_cast<Unsigned>(value)}
    {
        if (m_negative)
        {
            m_value = (~m_value) + 1;
        }
    }

    const Unsigned& Data() const noexcept
    {
        return m_value;
    }

    bool Negative() const noexcept
    {
        return m_negative;
    }


private:
    bool m_negative{};
    Unsigned m_value{};
};

class LogRecord
{
public:
    explicit LogRecord() = default;

    void Append(std::string_view text) noexcept
    {
        const auto count = std::min(text.size(), m_buffer.size() - m_size - 1);
        if (count != 0)
        {
            m_size += text.copy(m_buffer.data() + m_size, count);
        }
        m_truncated |= count < text.size();
    }

    void Append(const std::string& text) noexcept
    {
        Append(std::string_view{text});
    }

    void Append(const char* text) noexcept
    {
        size_t size = strnlen(text, BufferSize);
        Append(std::string_view{text, size});
    }

    template <size_t Size>
    void Append(const std::array<char, Size>& text) noexcept
    {
        size_t size = strnlen(text.data(), Size);
        Append(std::string_view{text.data(), size});
    }

    void Append(const void* pointer) noexcept
    {
        Append(Hex(reinterpret_cast<uintptr_t>(pointer)));
    }

    void Append(bool value) noexcept
    {
        Append(value ? "true" : "false");
    }

    template <std::integral Integer>
    void Append(const Hex<Integer>& value) noexcept
    {
        using DataType = Hex<Integer>::Unsigned;
        constexpr size_t DigitsCount = (std::numeric_limits<DataType>::digits + 3) / 4;
        std::array<char, DigitsCount + 1> number{};
        const auto result = std::to_chars(number.begin(), number.end(), value.Data(), 16);
        if (result.ec == std::errc{})
        {
            if (value.Negative())
            {
                Append("-");
            }
            Append("0x");
            Append(std::string_view{number.data(), result.ptr});
        }
    }

    template <std::integral Integer>
    void Append(const Binary<Integer>& value) noexcept
    {
        using DataType = Binary<Integer>::Unsigned;
        constexpr size_t DigitsCount = std::numeric_limits<DataType>::digits;
        std::array<char, DigitsCount + 1> number{};
        const auto result = std::to_chars(number.begin(), number.end(), value.Data(), 2);
        if (result.ec == std::errc{})
        {
            if (value.Negative())
            {
                Append("-");
            }
            Append("0b");
            Append(std::string_view{number.data(), result.ptr});
        }
    }

    void Append(float value) noexcept
    {
        std::array<char, 64> number{};
        const auto result =
            std::to_chars(number.data(), number.data() + number.size(), value, std::chars_format::fixed, 3);
        if (result.ec == std::errc{})
        {
            Append(std::string_view{number.data(), result.ptr});
        }
    }

    template <std::integral Integer>
    void Append(Integer value) noexcept
        requires(!std::same_as<Integer, bool>)
    {
        std::array<char, std::numeric_limits<Integer>::digits10 + 2> number{};
        const auto result = std::to_chars(number.data(), number.data() + number.size(), value);
        if (result.ec == std::errc{})
        {
            Append(std::string_view{number.data(), result.ptr});
        }
    }

    void Append(const std::source_location& location) noexcept
    {
        Append(location.function_name());
        Append(":");
        Append(location.line());
    }

    std::string_view Data() const noexcept
    {
        constexpr std::string_view TruncationMarker = " [truncated]";
        if (m_truncated)
        {
            TruncationMarker.copy(m_buffer.data() + m_size - TruncationMarker.size(), TruncationMarker.size());
        }
        m_buffer[m_size] = '\n';
        return {m_buffer.data(), m_size + 1};
    }

private:
    size_t m_size{};
    bool m_truncated{};

    static constexpr size_t BufferSize = 1024;
    ABSL_CONST_INIT static inline thread_local std::array<char, BufferSize> m_buffer{};
};

template <typename Part>
concept LogPart = requires(LogRecord& record, const Part& part) {
    { record.Append(part) } noexcept;
};

} // namespace memhawk::logging
