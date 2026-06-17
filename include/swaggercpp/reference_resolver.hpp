#pragma once

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"

namespace swaggercpp {

class SWAGGERCPP_EXPORT ReferenceResolver {
public:
    static void resolve(Document& document);
};

} // namespace swaggercpp
