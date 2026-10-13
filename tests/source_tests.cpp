#include <gtest/gtest.h>

#include <cmath>
#include <limits>

#include "swaggercpp/swaggercpp.hpp"

TEST(SourceDiagnostics, AttachesYamlSourceLocationsToSemanticIssues) {
    const auto input = "openapi: 3.1.0\ninfo: {title: Source, version: '1'}\npaths:\n  /pets/{id}:\n    get:\n      responses: {}\n";
    const auto parsed = swaggercpp::DocumentReader::read(input, {.track_source_locations = true});
    ASSERT_TRUE(parsed) << parsed.issues().front().message;
    const auto report = swaggercpp::DocumentValidator().validate(parsed.value());
    ASSERT_FALSE(report.ok());
    for (const auto& issue : report.issues) {
        ASSERT_TRUE(issue.line) << issue.path;
        EXPECT_GE(*issue.line, 4u);
        EXPECT_FALSE(issue.rule_id.empty());
    }
}

TEST(SourceDiagnostics, LocationsAreOptInForJsonAndYaml) {
    const std::string input = R"({"openapi":"3.1.0","info":{"title":"Source","version":"1"},"paths":{}})";
    const auto plain = swaggercpp::DocumentReader::read(input);
    ASSERT_TRUE(plain);
    EXPECT_TRUE(plain.value().source_locations.empty());
    const auto tracked = swaggercpp::DocumentReader::read(input, {.track_source_locations = true});
    ASSERT_TRUE(tracked);
    ASSERT_TRUE(tracked.value().source_locations.contains("/info/title"));
    EXPECT_EQ(tracked.value().source_locations.at("/info/title").line, 1u);
    EXPECT_GT(tracked.value().source_locations.at("/info/title").column, 1u);
}

TEST(SourceDiagnostics, ReportsSyntaxErrorPosition) {
    const auto parsed = swaggercpp::DocumentReader::read("{\n\"openapi\": \n}");
    ASSERT_FALSE(parsed);
    ASSERT_EQ(parsed.issues().size(), 1u);
    ASSERT_TRUE(parsed.issues().front().line);
    EXPECT_EQ(*parsed.issues().front().line, 3u);
}

TEST(DocumentInputSafety, EnforcesInputAndNestingLimits) {
    constexpr auto input = R"({"openapi":"3.1.0","info":{"title":"Source","version":"1"},"paths":{}})";
    EXPECT_FALSE(swaggercpp::DocumentReader::read(input, {.max_document_bytes = 5}));
    EXPECT_FALSE(swaggercpp::DocumentReader::read(input, {.max_nesting_depth = 1}));
    EXPECT_FALSE(swaggercpp::DocumentReader::read("openapi: 3.1.0\ninfo: {title: Source, version: '1'}\npaths: {}", {.max_nesting_depth = 1}));
    EXPECT_FALSE(swaggercpp::DocumentReader::read("openapi: 3.1.0\ninfo: &a {title: Source, version: '1', x-cycle: *a}\npaths: {}"));
    EXPECT_FALSE(swaggercpp::DocumentReader::read("openapi: 3.1.0\ninfo: {title: Source, title: Duplicate, version: '1'}\npaths: {}"));
    const std::string expanded = "openapi: 3.1.0\ninfo: {title: T, version: '1'}\npaths: {}\nx-text: &v "
        + std::string(100, 'a') + "\nx-values: [*v, *v, *v, *v, *v]";
    EXPECT_FALSE(swaggercpp::DocumentReader::read(expanded, {.max_document_bytes = 300}));
}

TEST(YamlFidelity, PreservesScalarTypesAndFloatingPointValues) {
    const auto input = R"({"openapi":"3.1.0","info":{"title":"true","version":"1"},"paths":{},"x-data":["false","123","null",18446744073709551615,1.25e-20,true,false,null]})";
    const auto parsed = swaggercpp::DocumentReader::read(input);
    ASSERT_TRUE(parsed);
    const auto yaml = swaggercpp::DocumentWriter::write_yaml(parsed.value());
    ASSERT_TRUE(yaml);
    const auto reloaded = swaggercpp::DocumentReader::read(yaml.value());
    ASSERT_TRUE(reloaded);
    const auto written = swaggercpp::DocumentWriter::write_json(reloaded.value());
    ASSERT_TRUE(written);
    EXPECT_EQ(nlohmann::json::parse(input), nlohmann::json::parse(written.value()));
}

TEST(YamlFidelity, ResolvesCoreSchemaNumericFormsWithoutChangingQuotedStrings) {
    const auto input = R"(
openapi: 3.1.0
info: {title: Core scalar types, version: "1"}
paths: {}
x-values: [.5, +12, 1., +12e03, -2E+05, 0x3A, 0o7, 0012, -19, +.5]
x-strings: [".5", "+12", "1.", "0x3A", "0o7", ".inf", nan, inf, yes, 0b10]
components:
  schemas:
    Number:
      minimum: .5
      maximum: +12
      multipleOf: 0o2
)";
    const auto parsed = swaggercpp::DocumentReader::read(input);
    ASSERT_TRUE(parsed) << parsed.issues().front().message;
    const nlohmann::json expected = {0.5, 12, 1.0, 12000.0, -200000.0, 58, 7, 12, -19, 0.5};
    EXPECT_EQ(parsed.value().extensions.at("x-values"), expected);
    for (const auto& value : parsed.value().extensions.at("x-strings")) EXPECT_TRUE(value.is_string()) << value.dump();
    const auto& schema = *parsed.value().components.schemas.at("Number");
    EXPECT_EQ(*schema.minimum, 0.5);
    EXPECT_EQ(*schema.maximum, 12);
    EXPECT_EQ(*schema.multiple_of, 2);
}

TEST(YamlFidelity, HonorsExplicitCoreTags) {
    const auto input = R"(
openapi: 3.1.0
info: {title: Explicit tags, version: "1"}
paths: {}
x-values: [!!float "12", !!float ".5", !!int "0o17", !!int "0xFF", !!int "+12", !!str true, !!bool "false", !!null "null"]
x-range: [!!int "18446744073709551615", !!int "-9223372036854775808"]
)";
    const auto parsed = swaggercpp::DocumentReader::read(input);
    ASSERT_TRUE(parsed) << parsed.issues().front().message;
    const auto& values = parsed.value().extensions.at("x-values");
    EXPECT_TRUE(values[0].is_number_float());
    EXPECT_EQ(values[0], 12.0);
    EXPECT_EQ(values[1], 0.5);
    EXPECT_EQ(values[2], 15);
    EXPECT_EQ(values[3], 255);
    EXPECT_EQ(values[4], 12);
    EXPECT_EQ(values[5], "true");
    EXPECT_EQ(values[6], false);
    EXPECT_TRUE(values[7].is_null());
    EXPECT_EQ(parsed.value().extensions.at("x-range")[0], std::numeric_limits<std::uint64_t>::max());
    EXPECT_EQ(parsed.value().extensions.at("x-range")[1], std::numeric_limits<std::int64_t>::min());
    for (const auto* value : {"!!int '1.5'", "!!float 'abc'", "!!bool 'yes'", "!!null 'abc'"}) {
        const auto invalid = swaggercpp::DocumentReader::read(std::string("openapi: 3.1.0\ninfo: {title: Tags, version: '1'}\npaths: {}\nx-value: ") + value);
        EXPECT_FALSE(invalid) << value;
    }
}

TEST(YamlFidelity, RejectsNonfiniteAndUnrepresentableNumbers) {
    const std::string prefix = "openapi: 3.1.0\ninfo: {title: Finite, version: '1'}\npaths: {}\nx-value: ";
    for (const auto* value : {".nan", ".NaN", ".NAN", ".inf", "-.Inf", "+.INF", "1e400", "18446744073709551616", "-9223372036854775809", "0x10000000000000000"}) {
        const auto parsed = swaggercpp::DocumentReader::read(prefix + value);
        EXPECT_FALSE(parsed) << value;
    }
    for (const auto* value : {"nan", "inf", "NaN", "Infinity", "'-.Inf'", "!!str .nan"}) {
        const auto parsed = swaggercpp::DocumentReader::read(prefix + value);
        ASSERT_TRUE(parsed) << value;
        EXPECT_TRUE(parsed.value().extensions.at("x-value").is_string()) << value;
    }
}

TEST(YamlFidelity, PreservesIntegralFloatsAndNegativeZeroAcrossYaml) {
    const auto parsed = swaggercpp::DocumentReader::read(R"({"openapi":"3.1.0","info":{"title":"Float identity","version":"1"},"paths":{},"x-values":[-0.0,0.0,1.0,1.2345678901234567]})");
    ASSERT_TRUE(parsed);
    const auto written = swaggercpp::DocumentWriter::write_yaml(parsed.value());
    ASSERT_TRUE(written);
    const auto reread = swaggercpp::DocumentReader::read(written.value());
    ASSERT_TRUE(reread) << written.value();
    const auto& values = reread.value().extensions.at("x-values");
    for (const auto& value : values) EXPECT_TRUE(value.is_number_float()) << written.value();
    EXPECT_TRUE(std::signbit(values[0].get<double>()));
    EXPECT_FALSE(std::signbit(values[1].get<double>()));
    EXPECT_EQ(values[2].get<double>(), 1.0);
    EXPECT_EQ(values[3].get<double>(), 1.2345678901234567);
}

TEST(DocumentWriterPresence, PreservesEmptyReferenceDescriptionOverride) {
    const auto input = R"({"openapi":"3.1.0","info":{"title":"T","version":"1"},"paths":{},"components":{"responses":{"R":{"$ref":"#/components/responses/Other","description":""},"Other":{"description":"base"}}}})";
    const auto parsed = swaggercpp::DocumentReader::read(input);
    ASSERT_TRUE(parsed);
    const auto written = swaggercpp::DocumentWriter::write_json(parsed.value());
    ASSERT_TRUE(written);
    EXPECT_EQ(nlohmann::json::parse(input), nlohmann::json::parse(written.value()));
}
