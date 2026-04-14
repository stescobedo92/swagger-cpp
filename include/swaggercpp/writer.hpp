#pragma once

#include <filesystem>
#include <string>

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"
#include "swaggercpp/result.hpp"

namespace swaggercpp {

struct WriterOptions {
    DocumentFormat format {DocumentFormat::yaml};
    bool pretty {true};
    int indent {2};
};

class SWAGGERCPP_EXPORT DocumentWriter {
public:
    [[nodiscard]] static Result<std::string> write(const Document& document, WriterOptions options = {});
    [[nodiscard]] static Result<std::string> write_json(const Document& document, bool pretty = true, int indent = 2);
    [[nodiscard]] static Result<std::string> write_yaml(const Document& document);
    [[nodiscard]] static Result<Document> roundtrip(std::string_view content, DocumentFormat output_format);
    static Result<std::string> write_file(const Document& document, const std::filesystem::path& path, WriterOptions options = {});
};

} // namespace swaggercpp

