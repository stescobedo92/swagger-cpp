# DocumentValidator

`DocumentValidator::builtin_rule_ids()` returns the supported built-in rule identifiers. The CLI rejects unknown IDs so configuration typos do not silently disable intended checks.

Header: `swaggercpp/validator.hpp`

Validates the typed document after parsing. It checks structural consistency and selected schema keywords; it is not a complete OpenAPI metaschema validator or a request/response payload validator. Empty paths and unconstrained schemas are valid. OpenAPI 3.1 documents may contain only webhooks or reusable components.

## API

```cpp
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
    explicit DocumentValidator(ValidatorOptions options = {});
    void add_rule(std::string rule_id, ValidationRule rule);
    [[nodiscard]] ValidationReport validate(const Document&) const;
};
```

Existing `DocumentValidator validator; validator.validate(document);` usage remains supported. `ValidationReport::ok()` is true when no issue has error severity. Validation does not mutate or resolve the document.

## Built-in rules

Structural rules are enabled as errors by default:

| Rule ID | Check |
| --- | --- |
| `document.version` | Exactly one Swagger 2.0, OpenAPI 3.0.x, or OpenAPI 3.1.x declaration, consistent with `Document::version` |
| `document.webhooks.version` | Webhooks require OpenAPI 3.1 |
| `document.depth` | Schema and preserved JSON traversal stays within a nesting limit of 256 |
| `document.limit` | Validation stops after 100,000 schema visits, including repeated visits to shared nodes |
| `info.title`, `info.version` | Nonempty API title and version |
| `path.leading_slash` | Path keys start with `/`; webhook names are exempt |
| `path.template` | Path template expressions are well formed |
| `path.parameter` | Declared path parameters match template expressions for each operation |
| `parameter.name` | Parameter names are nonempty |
| `parameter.path.required` | Path parameters are required |
| `parameter.schema_or_content` | Parameters define exactly one of schema or content; Swagger 2.0 parameters may define a top-level type |
| `parameter.content` | Parameter content contains only one media type |
| `parameter.unique` | Name/location pairs are unique within each parameter list |
| `operation.responses` | Every operation declares at least one response |
| `operation.id.unique` | Declared operation IDs are unique |
| `response.status` | Response keys are `100`–`599` or `default`; OpenAPI additionally permits `1XX`–`5XX` |
| `response.description` | Non-reference responses declare a description; explicitly empty strings are valid |
| `security.scheme` | Document and operation requirements refer to defined security schemes |
| `reference.unresolved` | Fragment references resolve, including escaped/percent-encoded JSON pointers and schema anchors |
| `schema.version` | Boolean schemas, type arrays, and exclusive bounds match the declared specification version |
| `schema.type` | Schema types are supported and type-array values are unique |
| `schema.numeric` | Numeric bounds and `multipleOf` contain numeric values |
| `schema.multiple_of` | `multipleOf` is positive |
| `schema.required.unique` | Required property names are unique |

The validator traverses reusable schemas, parameters, responses, and request bodies as well as operations and webhooks. Recursive schema graphs terminate safely. Repeated expansion of a shared acyclic schema graph is bounded by a 100,000-visit limit; exceeding it emits one `document.limit` issue and stops further schema traversal and reference checking. The report is incomplete in this case. Path-level parameters may be overridden by operation-level parameters. Internal component parameter references participate in duplicate and template checks. External references are left to the reference resolver; unresolved external parameter definitions do not produce speculative missing-template-parameter errors.

Optional lint rules default to warnings and are disabled unless enabled through their convenience option or the rule map:

| Rule ID | Convenience option | Check |
| --- | --- | --- |
| `operation.id.required` | `require_operation_id` | Every operation has a nonempty operation ID |
| `operation.description.required` | `require_operation_description` | Every operation has a description |
| `operation.error_response.required` | `require_error_response` | Every operation documents a 4xx, 5xx, or default response |

## Configure rules

```cpp
swaggercpp::ValidatorOptions options;
options.require_operation_id = true;
options.require_error_response = true;
options.rules["operation.id.required"].severity = swaggercpp::IssueSeverity::error;
options.rules["response.description"].enabled = false;

swaggercpp::DocumentValidator validator(options);
const auto report = validator.validate(document);
```

A rule-map entry overrides the convenience option. Creating an entry with `enabled = true` also enables an optional lint rule. Unknown rule IDs have no effect unless registered as custom rules.

## Custom rules

```cpp
validator.add_rule("team.contact", [](const swaggercpp::Document& document) {
    std::vector<swaggercpp::Issue> issues;
    if (!document.info.contact) {
        issues.push_back({
            .severity = swaggercpp::IssueSeverity::warning,
            .path = "/info/contact",
            .message = "Provide a support contact"
        });
    }
    return issues;
});
```

The registered rule ID is attached to each returned issue. Its configuration can disable the callback entirely or override the severity of all its issues. Empty IDs, empty callbacks, and duplicate custom rule IDs throw `std::invalid_argument`. Callback exceptions propagate to the caller. Use a project-specific prefix for custom IDs to avoid collisions with built-in IDs.

## Diagnostics and source locations

Every built-in issue includes a stable `rule_id` and an escaped JSON Pointer `path`, such as `/paths/~1items/get/responses/200/description`. Issues are sorted by path, rule ID, message, and severity so command-line and CI output is deterministic.

Read with `ReaderOptions::track_source_locations = true` to retain source coordinates. The validator adds `Document::source_file` and the recorded location of the issue's node, or its nearest existing ancestor for a missing field. It leaves line and column unset when the reader did not record them. Coordinates provided by custom rules are preserved; validation never guesses missing coordinates.

```cpp
swaggercpp::ReaderOptions reader_options;
reader_options.track_source_locations = true;
const auto result = swaggercpp::DocumentReader::read_file("spec.yaml", reader_options);
if (!result) return 1;

const auto report = swaggercpp::DocumentValidator().validate(result.value());
for (const auto& issue : report.issues) {
    std::cerr << issue.rule_id << ": " << issue.path << ": " << issue.message << '\n';
}
if (!report.ok()) return 2;
```

## Reference scope and limits

Fragment references are checked against the current document or the enclosing schema resource created by `$id`. Schema `$anchor` names are recognized. Unknown fields and bundled reference targets remain available for JSON Pointer lookup. Examples, defaults, and enum/constant payloads are treated as data, not reference declarations. This pass does not fetch external documents, validate OAuth scope catalogs, or exhaustively validate every keyword retained in raw JSON fields.

## See also

- [DocumentReader](/api/reader)
- [Result and Issues](/api/result)
- [ReferenceResolver](/api/references)
- [Validate example](/examples/validate-document)
