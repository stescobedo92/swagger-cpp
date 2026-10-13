#include <charconv>
#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>

#include "swaggercpp/swaggercpp.hpp"
#include "detail/yaml_utils.hpp"

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace {
using namespace swaggercpp;

constexpr std::string_view usage =
    "swaggercpp-cli " SWAGGERCPP_VERSION "\n"
    "Usage:\n"
    "  swaggercpp-cli validate <file> [--json] [--strict] [--config <rules.json>]\n"
    "  swaggercpp-cli convert <file> --format json|yaml [--output <file>]\n"
    "  swaggercpp-cli bundle <file> [--format json|yaml] [--output <file>] [--root <directory>]\n"
    "  swaggercpp-cli serve <file> [--port <0..65535>] [--no-browser]\n"
    "  swaggercpp-cli --help | --version\n"
    "Exit codes: 0 success, 1 input/output failure, 2 validation failure, 64 usage error.\n";

struct Arguments {
    std::string command;
    std::filesystem::path input;
    std::map<std::string, std::string> values;
    std::set<std::string> flags;
};

Arguments parse_arguments(int argc, char** argv) {
    if (argc < 3) throw std::invalid_argument("A command and input file are required");
    Arguments args {argv[1], std::filesystem::path(argv[2]), {}, {}};
    std::set<std::string> value_options;
    std::set<std::string> flag_options;
    if (args.command == "validate") {
        value_options = {"--config"};
        flag_options = {"--json", "--strict"};
    } else if (args.command == "convert") {
        value_options = {"--format", "--output"};
    } else if (args.command == "bundle") {
        value_options = {"--format", "--output", "--root"};
    } else if (args.command == "serve") {
        value_options = {"--port"};
        flag_options = {"--no-browser"};
    } else {
        throw std::invalid_argument("Unknown command: " + args.command);
    }
    for (int index = 3; index < argc; ++index) {
        const std::string option = argv[index];
        if (args.values.contains(option) || args.flags.contains(option)) {
            throw std::invalid_argument("Duplicate option: " + option);
        }
        if (flag_options.contains(option)) {
            args.flags.insert(option);
        } else if (value_options.contains(option)) {
            if (++index == argc || std::string_view(argv[index]).starts_with("--")) {
                throw std::invalid_argument("Missing value for " + option);
            }
            args.values.emplace(option, argv[index]);
        } else {
            throw std::invalid_argument("Unknown option: " + option);
        }
    }
    if (args.command == "convert" && !args.values.contains("--format")) {
        throw std::invalid_argument("convert requires --format json or yaml");
    }
    if (const auto it = args.values.find("--format"); it != args.values.end()
        && it->second != "json" && it->second != "yaml") {
        throw std::invalid_argument("--format must be json or yaml");
    }
    return args;
}

void print_issues(const std::vector<Issue>& issues, const std::filesystem::path& source, bool json_output, bool ok) {
    if (json_output) {
        auto entries = nlohmann::json::array();
        for (const auto& issue : issues) {
            nlohmann::json entry {
                {"severity", issue.severity == IssueSeverity::error ? "error" : "warning"},
                {"path", issue.path}, {"message", issue.message}, {"ruleId", issue.rule_id},
                {"file", issue.source_file.empty() ? source.string() : issue.source_file}};
            if (issue.line) entry["line"] = *issue.line;
            if (issue.column) entry["column"] = *issue.column;
            entries.push_back(std::move(entry));
        }
        std::cout << nlohmann::json {{"ok", ok}, {"issues", entries}}.dump(2) << '\n';
        return;
    }
    for (const auto& issue : issues) {
        std::cerr << (issue.source_file.empty() ? source.string() : issue.source_file);
        if (issue.line) std::cerr << ':' << *issue.line;
        if (issue.column) std::cerr << ':' << *issue.column;
        std::cerr << ": " << (issue.severity == IssueSeverity::error ? "error" : "warning");
        if (!issue.rule_id.empty()) std::cerr << " [" << issue.rule_id << ']';
        std::cerr << ' ' << issue.path << ": " << issue.message << '\n';
    }
}

ValidatorOptions load_rules(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("Cannot open rule configuration: " + path);
    std::string content;
    std::array<char, 8192> buffer {};
    while (stream.read(buffer.data(), static_cast<std::streamsize>(buffer.size())) || stream.gcount() != 0) {
        const auto count = static_cast<std::size_t>(stream.gcount());
        if (count > 1024 * 1024 - content.size()) throw std::invalid_argument("Rule configuration exceeds 1 MiB");
        content.append(buffer.data(), count);
    }
    if (stream.bad()) throw std::runtime_error("Cannot read rule configuration: " + path);
    nlohmann::json config;
    try {
        config = nlohmann::json::parse(content, [](int depth, nlohmann::json::parse_event_t, nlohmann::json&) {
            if (depth > 16) throw std::invalid_argument("Rule configuration nesting exceeds 16 levels");
            return true;
        });
    } catch (const nlohmann::json::exception& error) {
        throw std::invalid_argument("Invalid rule configuration: " + std::string(error.what()));
    }
    if (!config.is_object()) throw std::invalid_argument("Rule configuration must be an object");
    ValidatorOptions result;
    for (const auto& [key, value] : config.items()) {
        if (key == "rules") {
            if (!value.is_object()) throw std::invalid_argument("rules must be an object");
            for (const auto& [id, settings] : value.items()) {
                if (id.empty() || !settings.is_object()) throw std::invalid_argument("Invalid rule settings");
                if (!std::ranges::binary_search(DocumentValidator::builtin_rule_ids(), std::string_view(id))) {
                    throw std::invalid_argument("Unknown validation rule: " + id);
                }
                auto& rule = result.rules[id];
                for (const auto& [setting, setting_value] : settings.items()) {
                    if (setting == "enabled" && setting_value.is_boolean()) {
                        rule.enabled = setting_value.get<bool>();
                    } else if (setting == "severity" && setting_value.is_string()) {
                        const auto severity = setting_value.get<std::string>();
                        if (severity != "error" && severity != "warning") {
                            throw std::invalid_argument("Rule severity must be error or warning");
                        }
                        rule.severity = severity == "error" ? IssueSeverity::error : IssueSeverity::warning;
                    } else {
                        throw std::invalid_argument("Unknown or invalid rule setting: " + setting);
                    }
                }
            }
        } else if (value.is_boolean() && key == "requireOperationId") {
            result.require_operation_id = value.get<bool>();
        } else if (value.is_boolean() && key == "requireOperationDescription") {
            result.require_operation_description = value.get<bool>();
        } else if (value.is_boolean() && key == "requireErrorResponse") {
            result.require_error_response = value.get<bool>();
        } else {
            throw std::invalid_argument("Unknown or invalid configuration key: " + key);
        }
    }
    return result;
}

void write_output(const Arguments& args, const std::string& content) {
    if (const auto it = args.values.find("--output"); it != args.values.end()) {
        const std::filesystem::path target(it->second);
        std::filesystem::path staged;
        std::ofstream stream;
        std::random_device random;
        for (int attempt = 0; attempt < 8 && !stream.is_open(); ++attempt) {
            staged = target;
            staged += "." + std::to_string(random()) + ".tmp";
            stream.open(staged, std::ios::binary | std::ios::out | std::ios::noreplace);
        }
        if (!stream.is_open()) throw std::runtime_error("Cannot create output file beside: " + it->second);
        try {
            stream << content << '\n';
            stream.close();
            if (!stream) throw std::runtime_error("Cannot write output file: " + it->second);
#ifndef _WIN32
            const auto previous = std::filesystem::status(target);
            if (std::filesystem::is_regular_file(previous)) {
                std::filesystem::permissions(staged, previous.permissions());
            }
#endif
#ifdef _WIN32
            if (!MoveFileExW(staged.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
                throw std::runtime_error("Cannot replace output file: " + it->second);
            }
#else
            std::filesystem::rename(staged, target);
#endif
        } catch (...) {
            stream.close();
            std::error_code ignored;
            std::filesystem::remove(staged, ignored);
            throw;
        }
    } else {
        std::cout << content << '\n';
        if (!std::cout) throw std::runtime_error("Cannot write standard output");
    }
}

int run(const Arguments& args) {
    if (args.command == "bundle") {
        ResolverOptions options;
        if (args.values.contains("--root")) options.allowed_root = args.values.at("--root");
        const auto bundled = ReferenceResolver::bundle_json_file(args.input, options);
        if (!bundled) {
            print_issues(bundled.issues(), args.input, false, false);
            return 1;
        }
        const bool yaml = args.values.contains("--format") && args.values.at("--format") == "yaml";
        write_output(args, yaml ? detail::emit_yaml(bundled.value()) : bundled.value().dump(2));
        return 0;
    }
    const bool json_report = args.flags.contains("--json");
    const auto document = DocumentReader::read_file(args.input, {.track_source_locations = args.command == "validate"});
    if (!document) {
        print_issues(document.issues(), args.input, json_report, false);
        return 1;
    }
    if (args.command == "validate") {
        auto options = args.values.contains("--config") ? load_rules(args.values.at("--config")) : ValidatorOptions {};
        const auto report = DocumentValidator(std::move(options)).validate(document.value());
        auto issues = document.issues();
        issues.insert(issues.end(), report.issues.begin(), report.issues.end());
        const bool ok = report.ok() && !(args.flags.contains("--strict") && !issues.empty());
        print_issues(issues, args.input, json_report, ok);
        if (ok && !json_report) std::cout << "Valid: " << args.input.string() << '\n';
        return ok ? 0 : 2;
    }
    print_issues(document.issues(), args.input, false, true);
    if (args.command == "convert") {
        const auto output = DocumentWriter::write(document.value(), {
            .format = args.values.at("--format") == "json" ? DocumentFormat::json : DocumentFormat::yaml});
        if (!output) {
            print_issues(output.issues(), args.input, false, false);
            return 1;
        }
        write_output(args, output.value());
        return 0;
    }
    SwaggerUiOptions options;
    options.open_browser = !args.flags.contains("--no-browser");
    if (const auto it = args.values.find("--port"); it != args.values.end()) {
        const auto& text = it->second;
        const auto result = std::from_chars(text.data(), text.data() + text.size(), options.port);
        if (result.ec != std::errc {} || result.ptr != text.data() + text.size()
            || options.port < 0 || options.port > 65535) {
            throw std::invalid_argument("--port must be between 0 and 65535");
        }
    }
    auto session = SwaggerUiServer::start(document.value(), options);
    if (!session) {
        print_issues(session.issues(), args.input, false, false);
        return 1;
    }
    std::cout << session.value().ui_url() << std::endl;
    session.value().wait();
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    bool json_report = false;
    std::filesystem::path source;
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--help") {
            std::cout << usage;
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "--version") {
            std::cout << SWAGGERCPP_VERSION << '\n';
            return 0;
        }
        const auto arguments = parse_arguments(argc, argv);
        json_report = arguments.flags.contains("--json");
        source = arguments.input;
        return run(arguments);
    } catch (const std::invalid_argument& error) {
        if (json_report) print_issues({Issue {.message = error.what(), .rule_id = "cli.configuration"}}, source, true, false);
        else std::cerr << error.what() << '\n' << usage;
        return 64;
    } catch (const std::exception& error) {
        if (json_report) print_issues({Issue {.message = error.what(), .rule_id = "cli.io"}}, source, true, false);
        else std::cerr << error.what() << '\n';
        return 1;
    }
}
