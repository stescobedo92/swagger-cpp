#pragma once

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"
#include "swaggercpp/result.hpp"

namespace swaggercpp {

class SWAGGERCPP_EXPORT DocumentValidator {
public:
    [[nodiscard]] ValidationReport validate(const Document& document) const;
};

} // namespace swaggercpp

