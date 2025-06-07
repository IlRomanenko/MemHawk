#pragma once

#include <boost/algorithm/string/find.hpp>
#include <boost/algorithm/string/finder.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/hana/accessors.hpp>
#include <boost/hana/for_each.hpp>

#include <string>
#include <string_view>
#include <type_traits>

namespace memhawk
{
namespace config
{

enum class ParseErrorType : uint8_t
{
    Ok = 0,
    FieldNotFound,
    ValueNotParsed,
    ValueNotFound,
    RequiredFieldMissed,
};

struct ParseError
{
    ParseErrorType type;
    std::string_view key;
    std::string_view field;
    std::string_view value;

    bool operator==(const ParseError&) const = default;
};

using ErrorCallback = std::function<void(const ParseError&)>;

struct ParseContext
{
    std::string_view keyView;
    uint32_t parsedSize{};
    std::string_view valueView;
    ErrorCallback onError;
};

constexpr const char* OptionDelim = ":";
constexpr const char* OptionKeyValueDelim = "=";
constexpr const char* OptionNestedDelim = ".";

inline ParseErrorType GetErrorType(bool fieldFound, bool valueParsed)
{
    if (!fieldFound)
    {
        return ParseErrorType::FieldNotFound;
    }
    if (!valueParsed)
    {
        return ParseErrorType::ValueNotParsed;
    }
    return ParseErrorType::Ok;
}

template <typename T>
inline bool ParseStructField(const ParseContext& ctx, T& object)
{
    const auto fieldKey = ctx.keyView.substr(ctx.parsedSize);
    bool fieldFound = false;
    bool valueParsed = false;
    boost::hana::for_each(boost::hana::accessors<T>(), [&](const auto& pair) {
        auto& member = boost::hana::second(pair)(object);
        if (member.GetName() == fieldKey)
        {
            fieldFound = true;
            if constexpr (!boost::hana::Struct<std::decay_t<decltype(*member)>>::value)
            {
                valueParsed |= member.ParseValue(ctx.valueView);
            }
        }
    });
    const auto errorType = GetErrorType(fieldFound, valueParsed);
    if (errorType != ParseErrorType::Ok)
    {
        if (ctx.onError)
        {
            ctx.onError(ParseError{.type = errorType, .key = ctx.keyView, .field = fieldKey, .value = ctx.valueView});
        }
    }
    return errorType == ParseErrorType::Ok;
}

template <typename T>
inline bool ParseStructNested(const ParseContext& ctx, T& object)
{
    const auto splitPos = ctx.keyView.find(OptionNestedDelim, ctx.parsedSize);
    if (splitPos == std::string_view::npos)
    {
        return ParseStructField(ctx, object);
    }
    const auto structKey = ctx.keyView.substr(ctx.parsedSize, splitPos - ctx.parsedSize);

    auto nextCtx = ctx;
    nextCtx.parsedSize = splitPos + 1;

    bool fieldFound = false;
    bool valueParsed = false;
    boost::hana::for_each(boost::hana::accessors<T>(), [&](const auto& pair) {
        auto& member = boost::hana::second(pair)(object);
        if (member.GetName() == structKey)
        {
            fieldFound = true;
            if constexpr (boost::hana::Struct<std::decay_t<decltype(*member)>>::value)
            {
                const bool isParsed = ParseStructNested(nextCtx, *member);
                if (isParsed)
                {
                    member.SetFlag();
                }
                valueParsed |= isParsed;
            }
        }
    });
    if (!fieldFound)
    {
        if (ctx.onError)
        {
            ctx.onError(ParseError{
                .type = ParseErrorType::FieldNotFound, .key = ctx.keyView, .field = structKey, .value = ctx.valueView});
        }
    }
    return fieldFound && valueParsed;
}

template <typename T>
inline bool ValidateStruct(T& object, const ErrorCallback& onError = nullptr, const std::string& keyPath = "")
{
    bool valid = true;
    boost::hana::for_each(boost::hana::accessors<T>(), [&](const auto& pair) {
        auto& member = boost::hana::second(pair)(object);
        const auto memberName = member.GetName();
        auto fullPath = keyPath + OptionNestedDelim + memberName;
        if (keyPath.empty())
        {
            fullPath = memberName;
        }
        if constexpr (boost::hana::Struct<std::decay_t<decltype(*member)>>::value)
        {
            const bool nestedValid = ValidateStruct(*member, onError, fullPath);
            valid &= nestedValid;
        }
        else
        {
            if (!member.IsValid())
            {
                if (onError)
                {
                    onError(ParseError{.type = ParseErrorType::RequiredFieldMissed,
                                       .key = fullPath,
                                       .field = memberName,
                                       .value = {}});
                }
                valid = false;
            }
        }
    });
    return valid;
}

struct DescribeContext
{
    std::string_view key;
    std::string_view field;
    std::string_view defaultValue;
};

template <typename T>
inline void DescribeStruct(T& object, std::function<void(const DescribeContext&)> onKey, const std::string& keyPath = "")
{
    boost::hana::for_each(boost::hana::accessors<T>(), [&](const auto& pair) {
        auto& member = boost::hana::second(pair)(object);
        const auto memberName = member.GetName();
        auto fullPath = keyPath + OptionNestedDelim + memberName;
        if (keyPath.empty())
        {
            fullPath = memberName;
        }
        if constexpr (boost::hana::Struct<std::decay_t<decltype(*member)>>::value)
        {
            DescribeStruct(*member, onKey, fullPath);
        }
        else
        {
            onKey({.key = fullPath, .field = memberName, .defaultValue = member.GetDefaultValue()});
        }
    });
}

template <typename T>
inline bool ParseStruct(const std::string& input, T& object, const ErrorCallback& onError = nullptr)
{
    bool valid = true;
    const auto strView = std::string_view(input);
    auto splitIt = boost::algorithm::make_split_iterator(strView, boost::algorithm::first_finder(OptionDelim));
    for (; splitIt != decltype(splitIt)(); splitIt++)
    {
        const auto elemView = std::string_view(splitIt->begin(), splitIt->size());
        auto separatorPos = elemView.find(OptionKeyValueDelim);
        if (separatorPos == std::string_view::npos)
        {
            if (onError)
            {
                onError(
                    ParseError{.type = ParseErrorType::ValueNotFound, .key = elemView, .field = elemView, .value = {}});
            }
            valid = false;
            continue;
        }
        const auto keyView = elemView.substr(0, separatorPos);
        const auto valueView = elemView.substr(separatorPos + 1);

        const auto ctx = ParseContext{.keyView = keyView, .parsedSize = 0, .valueView = valueView, .onError = onError};
        valid &= ParseStructNested(ctx, object);
    }
    return valid & ValidateStruct(object, onError);
}

} // namespace config
} // namespace memhawk
