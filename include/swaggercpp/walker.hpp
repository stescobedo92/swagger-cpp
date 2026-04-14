#pragma once

#include <string_view>

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"

namespace swaggercpp {

class SWAGGERCPP_EXPORT DocumentVisitor {
public:
    virtual ~DocumentVisitor() = default;

    virtual void enter_document(const Document&) {}
    virtual void leave_document(const Document&) {}
    virtual void enter_path_item(std::string_view, const PathItem&) {}
    virtual void leave_path_item(std::string_view, const PathItem&) {}
    virtual void enter_operation(std::string_view, HttpMethod, const Operation&) {}
    virtual void leave_operation(std::string_view, HttpMethod, const Operation&) {}
    virtual void visit_parameter(const Parameter&) {}
    virtual void visit_response(std::string_view, const Response&) {}
    virtual void visit_schema(const Schema&, std::string_view) {}
};

class SWAGGERCPP_EXPORT DocumentWalker {
public:
    static void walk(const Document& document, DocumentVisitor& visitor);
};

} // namespace swaggercpp

