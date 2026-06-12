#include "config.h"
#include "impl/config/config_var.h"
#include "impl/config/parse_struct.h"

#include <boost/hana.hpp>
#include <gmock/gmock-actions.h>
#include <gmock/gmock-matchers.h>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <cstdint>
#include <string>

using namespace testing;

struct SmallConfig
{
    CONFIG_VAR(std::string, name, "name", true, "");
    CONFIG_VAR(uint32_t, value, "value", true, 0);

    bool operator==(const SmallConfig&) const = default;
};

BOOST_HANA_ADAPT_STRUCT(SmallConfig, name, value);

struct AppConfig
{
    CONFIG_VAR(bool, enabled, "enabled", true, false);
    CONFIG_VAR(SmallConfig, cfg, "cfg", true, {});

    bool operator==(const AppConfig&) const = default;
};

BOOST_HANA_ADAPT_STRUCT(AppConfig, enabled, cfg);

namespace memhawk
{

TEST(Config, ParseStructField_ExpectOk)
{
    SmallConfig cfg{};
    const config::ParseContext ctx{.keyView = "name", .parsedSize = 0, .valueView = "qwerty", .onError = nullptr};
    auto res = config::ParseStructField(ctx, cfg);
    EXPECT_TRUE(res);
}

TEST(Config, ParseStructField_ExpectFieldNotFound)
{
    constexpr const auto expError = config::ParseError{
        .type = config::ParseErrorType::FieldNotFound, .key = "abc", .field = "abc", .value = "qwerty"};

    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(Eq(expError))).WillOnce(Return());
    SmallConfig cfg{};
    const config::ParseContext ctx{
        .keyView = "abc", .parsedSize = 0, .valueView = "qwerty", .onError = mock.AsStdFunction()};
    auto res = config::ParseStructField(ctx, cfg);
    EXPECT_FALSE(res);
}

TEST(Config, ParseStructField_ExpectValueNotParsed)
{
    constexpr const auto expError = config::ParseError{
        .type = config::ParseErrorType::ValueNotParsed, .key = "value", .field = "value", .value = "qwerty"};

    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(Eq(expError))).WillOnce(Return());
    SmallConfig cfg{};
    const config::ParseContext ctx{
        .keyView = "value", .parsedSize = 0, .valueView = "qwerty", .onError = mock.AsStdFunction()};
    auto res = config::ParseStructField(ctx, cfg);
    EXPECT_FALSE(res);
}

TEST(Config, ParseStructNested_ExpectOk)
{
    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    SmallConfig cfg{};
    const config::ParseContext ctx{
        .keyView = "name", .parsedSize = 0, .valueView = "qwerty", .onError = mock.AsStdFunction()};
    auto res = config::ParseStructNested(ctx, cfg);
    EXPECT_TRUE(res);
}

TEST(Config, ParseStruct_ExpectOk)
{
    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    SmallConfig cfg{};
    auto res = config::ParseStruct("name=qwerty:value=2", cfg, mock.AsStdFunction());
    EXPECT_TRUE(res);

    SmallConfig exp{};
    exp.name = "qwerty";
    exp.value = 2;
    EXPECT_EQ(cfg, exp);
}

TEST(Config, ParseStruct_MissingRequiredField_ExpectError)
{
    constexpr const auto expError = config::ParseError{
        .type = config::ParseErrorType::RequiredFieldMissing, .key = "cfg.value", .field = "value", .value = ""};

    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(Eq(expError))).WillOnce(Return());
    AppConfig cfg{};
    auto res = config::ParseStruct("enabled=false:cfg.name=qwerty", cfg, mock.AsStdFunction());
    EXPECT_FALSE(res);
}

TEST(Config, ParseStruct_WithNested_ExpectOk)
{
    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    AppConfig cfg{};
    auto res = config::ParseStruct("enabled=true:cfg.name=qwerty:cfg.value=2", cfg, mock.AsStdFunction());
    EXPECT_TRUE(res);

    AppConfig exp{};
    exp.enabled = true;
    SmallConfig smallCfg{};
    smallCfg.name = "qwerty";
    smallCfg.value = 2;
    exp.cfg = smallCfg;
    EXPECT_EQ(cfg, exp);
}

TEST(Config, ParseStruct_WithNested_Comparable_ExpectOk)
{
    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    AppConfig cfg{};
    auto res = config::ParseStruct("enabled=true:cfg.name=qwerty:cfg.value=2", cfg, mock.AsStdFunction());
    EXPECT_TRUE(res);

    EXPECT_EQ(*cfg.enabled, true);
    EXPECT_EQ(*cfg.cfg->name, "qwerty");
    EXPECT_EQ(*cfg.cfg->value, 2);
}

TEST(Config, ParseStruct_WithNested_ExpectAllTypesOfErrors)
{
    const auto expectedErrors = {
        config::ParseError{
            .type = config::ParseErrorType::FieldNotFound, .key = "cfg.name_", .field = "name_", .value = "qwerty"},
        config::ParseError{
            .type = config::ParseErrorType::ValueNotParsed, .key = "cfg.value", .field = "value", .value = "name"},
        config::ParseError{
            .type = config::ParseErrorType::ValueNotFound, .key = "enabledtrue", .field = "enabledtrue", .value = ""},
        config::ParseError{
            .type = config::ParseErrorType::RequiredFieldMissing, .key = "cfg.name", .field = "name", .value = ""},
        config::ParseError{
            .type = config::ParseErrorType::RequiredFieldMissing, .key = "cfg.value", .field = "value", .value = ""},
        config::ParseError{
            .type = config::ParseErrorType::RequiredFieldMissing, .key = "enabled", .field = "enabled", .value = ""},
    };
    MockFunction<config::ErrorCallback> mock;
    for (const auto& err : expectedErrors)
    {
        EXPECT_CALL(mock, Call(Eq(err))).WillOnce(Return());
    }
    AppConfig cfg{};
    auto res = config::ParseStruct("cfg.name_=qwerty:cfg.value=name:enabledtrue", cfg, mock.AsStdFunction());
    EXPECT_FALSE(res);
}

TEST(Config, ParseStruct_DeepNested_ExpectOk)
{
    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    MainConfig cfg{};
    auto res = config::ParseStruct("memhawk.writers.proto_writer.enabled=1", cfg, mock.AsStdFunction());
    EXPECT_TRUE(res);

    EXPECT_EQ(*cfg.MemHawk->Writers->ProtobufWriter->Enabled, true);
}

TEST(Config, ParseStruct_StringField_ExpectOk)
{
    MockFunction<config::ErrorCallback> mock;
    EXPECT_CALL(mock, Call(_)).Times(0);
    MainConfig cfg{};
    auto res = config::ParseStruct("progname_regex=deadbeefdeadbeefdeadbeef", cfg, mock.AsStdFunction());
    EXPECT_TRUE(res);
    EXPECT_EQ(*cfg.PrognameRegex, "deadbeefdeadbeefdeadbeef");
}

} // namespace memhawk
