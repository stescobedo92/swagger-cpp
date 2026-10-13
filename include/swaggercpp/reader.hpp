#pragma once

#include <filesystem>
#include <string_view>

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"
#include "swaggercpp/result.hpp"

namespace swaggercpp {

struct ReaderOptions {
    bool allow_yaml {true};
    bool strict_required_fields {true};
    bool track_source_locations {false};
    std::size_t max_document_bytes {16 * 1024 * 1024};
    std::size_t max_nesting_depth {128};
};

class SWAGGERCPP_EXPORT DocumentReader {
public:
    [[nodiscard]] static Result<Document> read(std::string_view content, ReaderOptions options = {});
    [[nodiscard]] static Result<Document> read_file(const std::filesystem::path& path, ReaderOptions options = {});
};

} // namespace swaggercpp

