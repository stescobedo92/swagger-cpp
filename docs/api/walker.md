# DocumentWalker

Header: `swaggercpp/walker.hpp`

Deterministic visitor-pattern traversal of a `Document`. Implement only the callbacks you care about — defaults are no-ops.

## API

```cpp
class DocumentVisitor {
public:
    virtual ~DocumentVisitor() = default;

    virtual void enter_document(const Document&);
    virtual void leave_document(const Document&);

    virtual void enter_path_item(std::string_view path, const PathItem&);
    virtual void leave_path_item(std::string_view path, const PathItem&);

    virtual void enter_operation(std::string_view path, HttpMethod, const Operation&);
    virtual void leave_operation(std::string_view path, HttpMethod, const Operation&);

    virtual void visit_parameter(const Parameter&);
    virtual void visit_response(std::string_view status, const Response&);
    virtual void visit_schema(const Schema&, std::string_view jsonPointer);
};

class DocumentWalker {
public:
    static void walk(const Document&, DocumentVisitor&);
};
```

## Traversal order

For a `Document`, `walk` invokes callbacks in this order:

1. `enter_document`
2. For each `paths[key]`, ordered alphabetically:
   1. `enter_path_item(key, ...)`
   2. For each `operations[method]`:
      1. `enter_operation(key, method, ...)`
      2. `visit_parameter` for every parameter (path-level first, then operation-level)
      3. `visit_response` for each response code, ordered by status string
      4. `visit_schema` for schemas discovered in parameters, request body, and responses
      5. `leave_operation`
   3. `leave_path_item`
3. `leave_document`

::: tip
`visit_schema` is fired per distinct schema reference. Cyclic schemas (`all_of` loops) are visited once — the walker tracks visited nodes.
:::

## Example: counting methods

```cpp
struct MethodCounter : swaggercpp::DocumentVisitor {
    std::map<swaggercpp::HttpMethod, int> counts;

    void enter_operation(std::string_view,
                         swaggercpp::HttpMethod method,
                         const swaggercpp::Operation&) override {
        ++counts[method];
    }
};

MethodCounter visitor;
swaggercpp::DocumentWalker::walk(doc, visitor);

for (const auto& [m, n] : visitor.counts)
    std::cout << swaggercpp::to_string(m) << " = " << n << '\n';
```

## Example: emitting Markdown table of contents

```cpp
struct TocVisitor : swaggercpp::DocumentVisitor {
    std::ostringstream out;

    void enter_operation(std::string_view path,
                         swaggercpp::HttpMethod method,
                         const swaggercpp::Operation& op) override {
        out << "- **" << swaggercpp::to_string(method) << "** `" << path << "` — "
            << op.summary.value_or(op.operation_id.value_or("(unnamed)")) << '\n';
    }
};

TocVisitor v;
swaggercpp::DocumentWalker::walk(doc, v);
std::cout << v.out.str();
```

## See also

- [Traverse example](/examples/traverse-document)
- [Endpoint catalog example](/examples/endpoint-catalog) — more elaborate traversal
- [Document model](/api/document)
