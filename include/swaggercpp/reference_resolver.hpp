#pragma once

#include "swaggercpp/document.hpp"
#include "swaggercpp/export.hpp"
#include "swaggercpp/result.hpp"

#include <cstddef>
#include <filesystem>
#include <optional>

namespace swaggercpp {

struct ResolverOptions {
    std::size_t max_documents {64};
    std::size_t max_total_bytes {16 * 1024 * 1024};
    std::size_t max_depth {128};
    std::size_t max_nodes {1000000};
    // When unset, relative references may load any local regular file.
    std::optional<std::filesystem::path> allowed_root;
};

class SWAGGERCPP_EXPORT ReferenceResolver {
public:
    // Expands supported local component references without creating ownership cycles.
    // Recursive references and references with schema siblings remain references.
    static void resolve(Document& document);

    // Bundles JSON/YAML files without dropping fields or dereferencing cycles.
    [[nodiscard]] static Result<nlohmann::json> bundle_json_file(
        const std::filesystem::path& path, ResolverOptions options = {});
    [[nodiscard]] static Result<Document> bundle_file(
        const std::filesystem::path& path, ResolverOptions options = {});
};

} // namespace swaggercpp
