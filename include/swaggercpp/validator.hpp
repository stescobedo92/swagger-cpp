#pragma once

#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"
#include "swaggercpp/result.hpp"

namespace swaggercpp {

struct ValidationRuleOptions {
    bool enabled {true};
    std::optional<IssueSeverity> severity;
};

struct ValidatorOptions {
    bool require_operation_id {false};
    bool require_operation_description {false};
    bool require_error_response {false};
    std::map<std::string, ValidationRuleOptions> rules;
};

using ValidationRule = std::function<std::vector<Issue>(const Document&)>;

class DocumentValidator {
public:
    SWAGGERCPP_EXPORT explicit DocumentValidator(ValidatorOptions options = {});
    [[nodiscard]] SWAGGERCPP_EXPORT static std::span<const std::string_view> builtin_rule_ids() noexcept;

    // Custom rule IDs must be nonempty and unique within this validator.
    // Exceptions from custom callbacks propagate to the caller.
    SWAGGERCPP_EXPORT void add_rule(std::string rule_id, ValidationRule rule);
    [[nodiscard]] SWAGGERCPP_EXPORT ValidationReport validate(const Document& document) const;

private:
    ValidatorOptions options_;
    std::map<std::string, ValidationRule> custom_rules_;
};

} // namespace swaggercpp

