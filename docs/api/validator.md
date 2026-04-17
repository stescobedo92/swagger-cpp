# DocumentValidator

Header: `swaggercpp/validator.hpp`

Runs semantic checks on a `Document` that are independent of how the document was obtained. Parsing only catches syntactic issues — the validator catches the rest.

## API

```cpp
class DocumentValidator {
public:
    [[nodiscard]] ValidationReport validate(const Document&) const;
};

struct ValidationReport {
    std::vector<Issue> issues;
    [[nodiscard]] bool ok() const noexcept;   // true iff no errors
};
```

## What it checks

| Rule                                               | Severity |
|----------------------------------------------------|----------|
| `info.title` present and non-empty                 | error    |
| `info.version` present and non-empty               | error    |
| `openapi` or `swagger` declared                    | error    |
| Duplicate `operation_id` across operations         | error    |
| Response codes match `\d{3}` or equal `default`    | error    |
| Parameter `name` present and non-empty             | error    |
| Parameter `in` + `name` uniqueness per operation   | error    |
| In-document `$ref` targets exist                   | error    |
| `description` missing on responses                 | warning  |
| Operations without `operation_id`                  | warning  |
| Unused component schemas                           | warning  |

::: info
This list is the current baseline. As the library grows, more checks will be added. See the [Roadmap](https://github.com/stescobedo92/swagger-cpp#roadmap).
:::

## Usage

```cpp
swaggercpp::DocumentValidator validator;
const auto report = validator.validate(doc);

if (!report.ok()) {
    for (const auto& issue : report.issues) {
        if (issue.severity == swaggercpp::IssueSeverity::error) {
            std::cerr << "error: " << issue.path << ": " << issue.message << '\n';
        }
    }
    return 2;
}

// Print warnings even on success
for (const auto& issue : report.issues) {
    if (issue.severity == swaggercpp::IssueSeverity::warning) {
        std::clog << "warn:  " << issue.path << ": " << issue.message << '\n';
    }
}
```

## Combining with the reader

The typical pipeline is read → validate:

```cpp
auto r = swaggercpp::DocumentReader::read_file("spec.yaml");
if (!r) return 1;

swaggercpp::DocumentValidator v;
auto report = v.validate(r.value());
if (!report.ok()) return 2;
```

::: tip
The reader already reports syntax errors through `Result::issues()`. Validation is a second pass that assumes syntactic validity. You can skip it for trusted input, but it's cheap enough to run in most pipelines.
:::

## See also

- [DocumentReader](/api/reader)
- [Result and Issues](/api/result)
- [Validate example](/examples/validate-document)
