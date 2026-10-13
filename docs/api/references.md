# Reference resolution and bundling

`ReferenceResolver::bundle_json_file` combines a local OpenAPI JSON or YAML document
and its referenced files into one JSON value without discarding unmodeled fields.
`bundle_file` returns the same result through the typed `DocumentReader` model.

```cpp
#include <swaggercpp/swaggercpp.hpp>
#include <iostream>

swaggercpp::ResolverOptions options;
options.allowed_root = "spec";
options.max_documents = 64;
options.max_total_bytes = 16 * 1024 * 1024;

auto result = swaggercpp::ReferenceResolver::bundle_json_file("spec/api.yaml", options);
if (result) {
    std::cout << result.value().dump(2);
} else {
    for (const auto& issue : result.issues()) {
        std::cerr << issue.path << ": " << issue.message << '\n';
    }
}
```

References support relative file URIs, including percent-encoded filenames, and
JSON Pointer fragments with URI encoding and `~0`/`~1` escaping. Relative paths
are resolved from each referring file's directory. Each canonical file is read
once, and each referenced target is copied once per OpenAPI object type. Targets
are stored in a top-level `x-swaggercpp-bundled` extension; a suffix avoids
overwriting an existing extension. References point to those targets using local
JSON Pointers. Local references may also be relocated. Recursive references stay
as `$ref` values, so mutual recursion does not create an infinite JSON structure.

Traversal follows OpenAPI objects and standard schema keywords, including
`additionalProperties`, `$defs`, conditional schemas, and composition. Literal
data in `example`, `default`, `enum`, Example Object `value`, or custom extensions
is not interpreted as a schema or reference. Schema siblings beside `$ref` are
preserved. Root-local `operationRef` values in Link Objects are checked and kept;
external operation references are rejected because an operation must retain its
path and server context. Other URI fields, including discriminator mappings, `externalValue` and server URLs, remain unchanged;
their relative resource locations still need to be available to consumers.

The default limits are 64 input documents, 16 MiB total input bytes, 16 MiB copied
reference content, 128 levels of document/reference nesting, and 1,000,000 parsed
or visited nodes. YAML alias expansion is bounded by the node/depth limits and
16 MiB of expanded scalar/key content.
The copied-content limit is separate from the input-byte limit and uses the same
`max_total_bytes` setting. The original root document remains in the output, so
the output file can be larger than either individual byte limit.

With `allowed_root` unset, any local regular file reachable through relative
references can be read, including parent directories. Set `allowed_root` when
processing specifications from outside your project. Canonical paths are checked
after resolving symlinks and `..` segments. The input document must also be under
that root. Network paths, absolute reference paths, URI schemes, and query strings
are rejected. No HTTP requests are made.

Named anchors, schema `$id` scopes, dynamic/recursive-reference keywords, custom
vocabularies, and custom/older declared schema dialects are rejected with a
diagnostic instead of being rewritten with changed semantics. A selected fragment
cannot inherit an unsupported `$id` from an enclosing schema. Supported declared
dialects are OpenAPI 3.1's base dialect and JSON Schema 2020-12. Bundling resolves
references; use `DocumentValidator` separately to check the API contract.

`ReferenceResolver::resolve(Document&)` retains its existing signature and
expands supported in-memory `#/components/...` references. Recursive schema edges,
schemas with `$ref` siblings, and unsupported/unresolved references remain `$ref`
values. Schema subtrees with an explicit `$id` or schema dialect are left intact
because their reference base cannot be inferred from an in-memory document.
It does not load files and does not create `shared_ptr` ownership cycles.
Use the bundling APIs when diagnostics or external files are needed.

Reference semantics follow the [OpenAPI 3.1 relative-reference rules](https://spec.openapis.org/oas/v3.1.1.html#relative-references-in-api-description-uris).
