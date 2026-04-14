#pragma once

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace swaggercpp {

enum class IssueSeverity {
    warning,
    error
};

struct Issue {
    IssueSeverity severity {IssueSeverity::error};
    std::string path;
    std::string message;
};

template <typename T>
class Result {
public:
    Result() = default;

    static Result success(T value, std::vector<Issue> issues = {}) {
        Result result;
        result.value_ = std::move(value);
        result.issues_ = std::move(issues);
        return result;
    }

    static Result failure(std::vector<Issue> issues) {
        Result result;
        result.issues_ = std::move(issues);
        return result;
    }

    [[nodiscard]] bool ok() const noexcept {
        return value_.has_value() && !has_errors();
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return ok();
    }

    [[nodiscard]] bool has_errors() const noexcept {
        for (const auto& issue : issues_) {
            if (issue.severity == IssueSeverity::error) {
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] const T& value() const {
        if (!value_) {
            throw std::logic_error("Result does not contain a value");
        }
        return *value_;
    }

    [[nodiscard]] T& value() {
        if (!value_) {
            throw std::logic_error("Result does not contain a value");
        }
        return *value_;
    }

    [[nodiscard]] const std::vector<Issue>& issues() const noexcept {
        return issues_;
    }

private:
    std::optional<T> value_;
    std::vector<Issue> issues_;
};

struct ValidationReport {
    std::vector<Issue> issues;

    [[nodiscard]] bool ok() const noexcept {
        for (const auto& issue : issues) {
            if (issue.severity == IssueSeverity::error) {
                return false;
            }
        }
        return true;
    }
};

} // namespace swaggercpp

