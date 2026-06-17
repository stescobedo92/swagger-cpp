#include "swaggercpp/reference_resolver.hpp"
#include "swaggercpp/mutable_walker.hpp"
#include <string>
#include <string_view>

namespace swaggercpp {

namespace {

std::string unescape_json_pointer(std::string_view ptr) {
    std::string result;
    result.reserve(ptr.size());
    for (size_t i = 0; i < ptr.size(); ++i) {
        if (ptr[i] == '~' && i + 1 < ptr.size()) {
            if (ptr[i + 1] == '1') {
                result.push_back('/');
                ++i;
            } else if (ptr[i + 1] == '0') {
                result.push_back('~');
                ++i;
            } else {
                result.push_back(ptr[i]);
            }
        } else {
            result.push_back(ptr[i]);
        }
    }
    return result;
}

class ResolverVisitor : public MutableDocumentVisitor {
    Document& doc_;

public:
    explicit ResolverVisitor(Document& doc) : doc_(doc) {}

    void visit_schema(SchemaPtr& schema, std::string_view /*path*/) override {
        if (schema && schema->ref) {
            std::string ref = *schema->ref;
            if (ref.starts_with("#/components/schemas/")) {
                std::string name = unescape_json_pointer(ref.substr(21));
                if (auto it = doc_.components.schemas.find(name); it != doc_.components.schemas.end()) {
                    schema = it->second;
                }
            }
        }
    }

    void visit_parameter(Parameter& parameter) override {
        if (parameter.ref) {
            std::string ref = *parameter.ref;
            if (ref.starts_with("#/components/parameters/")) {
                std::string name = unescape_json_pointer(ref.substr(24));
                if (auto it = doc_.components.parameters.find(name); it != doc_.components.parameters.end()) {
                    parameter = it->second;
                }
            }
        }
    }

    void visit_response(std::string_view /*status*/, Response& response) override {
        if (response.ref) {
            std::string ref = *response.ref;
            if (ref.starts_with("#/components/responses/")) {
                std::string name = unescape_json_pointer(ref.substr(23));
                if (auto it = doc_.components.responses.find(name); it != doc_.components.responses.end()) {
                    response = it->second;
                }
            }
        }
    }

    void visit_request_body(RequestBody& request_body) override {
        if (request_body.ref) {
            std::string ref = *request_body.ref;
            if (ref.starts_with("#/components/requestBodies/")) {
                std::string name = unescape_json_pointer(ref.substr(27));
                if (auto it = doc_.components.request_bodies.find(name); it != doc_.components.request_bodies.end()) {
                    request_body = it->second;
                }
            }
        }
    }
};

} // namespace

void ReferenceResolver::resolve(Document& document) {
    ResolverVisitor visitor(document);
    MutableDocumentWalker::walk(document, visitor);
}

} // namespace swaggercpp
