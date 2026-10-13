#pragma once

#include <string_view>

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"

namespace swaggercpp {

class SWAGGERCPP_EXPORT MutableDocumentVisitor {
public:
    virtual ~MutableDocumentVisitor() = default;

    virtual void enter_document(Document&) {}
    virtual void leave_document(Document&) {}
    virtual void enter_path_item(std::string_view, PathItem&) {}
    virtual void leave_path_item(std::string_view, PathItem&) {}
    virtual void enter_operation(std::string_view, HttpMethod, Operation&) {}
    virtual void leave_operation(std::string_view, HttpMethod, Operation&) {}
    virtual void visit_parameter(Parameter&) {}
    virtual void visit_response(std::string_view, Response&) {}
    virtual void visit_request_body(RequestBody&) {}
    virtual void visit_schema(SchemaPtr&, std::string_view) {}
};

class SWAGGERCPP_EXPORT MutableDocumentWalker {
public:
    // Visits occurrences with cycle detection after pointer replacement.
    // Throws std::runtime_error beyond 100000 schema visits or depth 256.
    static void walk(Document& document, MutableDocumentVisitor& visitor);
};

} // namespace swaggercpp
