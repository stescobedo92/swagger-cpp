# Result and Issues

Header: `swaggercpp/result.hpp`

`swaggercpp` uses a return-type-based error channel instead of exceptions for expected failures.

## `Issue`

```cpp
enum class IssueSeverity { warning, error };

struct Issue {
    IssueSeverity severity { IssueSeverity::error };
    std::string   path;      // JSON Pointer-style path, e.g. "/paths/~1pets/get"
    std::string   message;   // human-readable description
};
```

`path` follows [RFC 6901](https://datatracker.ietf.org/doc/html/rfc6901) encoding: `/` is escaped as `~1`, `~` as `~0`. An empty path refers to the document root.

## `Result<T>`

```cpp
template <typename T>
class Result {
public:
    static Result success(T value, std::vector<Issue> issues = {});
    static Result failure(std::vector<Issue> issues);

    [[nodiscard]] bool ok() const noexcept;          // value present AND no errors
    [[nodiscard]] explicit operator bool() const noexcept;
    [[nodiscard]] bool has_errors() const noexcept;

    [[nodiscard]] const T&  value() const;           // throws logic_error if absent
    [[nodiscard]]       T&  value();

    [[nodiscard]] const std::vector<Issue>& issues() const noexcept;
};
```

### Semantics

| State             | `ok()` | `value()`       | `issues()`       |
|-------------------|--------|-----------------|------------------|
| Success, no issues | true  | returns value   | empty            |
| Success, warnings | true   | returns value   | warning entries  |
| Failure, with errors | false | **throws**   | error (+ warning) entries |

::: tip
`ok()` is stricter than "has a value" — it returns `false` when any issue has `IssueSeverity::error`. Use `if (result)` as the go-to guard; it already delegates to `ok()`.
:::

## `ValidationReport`

A lighter sibling used by `DocumentValidator`:

```cpp
struct ValidationReport {
    std::vector<Issue> issues;
    [[nodiscard]] bool ok() const noexcept;   // true iff no errors
};
```

There is no "value" here — validation's only output is the list of issues.

## Idioms

### Short-circuit on failure

```cpp
auto r = swaggercpp::DocumentReader::read_file("spec.yaml");
if (!r) return 1;
```

### Inspect issues on success

```cpp
for (const auto& issue : r.issues()) {
    // warnings only; errors already sent us down the failure path
}
```

### Accumulate across a pipeline

```cpp
std::vector<swaggercpp::Issue> collected;

auto r = swaggercpp::DocumentReader::read_file("spec.yaml");
collected.insert(collected.end(), r.issues().begin(), r.issues().end());

if (r) {
    swaggercpp::DocumentValidator v;
    auto report = v.validate(r.value());
    collected.insert(collected.end(), report.issues.begin(), report.issues.end());
}

std::ranges::sort(collected, {}, &swaggercpp::Issue::path);
```

## See also

- [Guide — the Result contract](/guide#the-result-t-contract)
- [DocumentReader](/api/reader)
- [DocumentValidator](/api/validator)
