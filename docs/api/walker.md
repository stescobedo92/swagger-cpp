# Document walkers

Headers: `swaggercpp/walker.hpp` and `swaggercpp/mutable_walker.hpp`.

The walkers visit the typed document model in deterministic order. Callback implementations are optional; the default implementations do nothing.

## Const visitor API

```cpp
class DocumentVisitor {
public:
    virtual ~DocumentVisitor() = default;
    virtual void enter_document(const Document&);
    virtual void leave_document(const Document&);
    virtual void enter_path_item(std::string_view name, const PathItem&);
    virtual void leave_path_item(std::string_view name, const PathItem&);
    virtual void enter_operation(std::string_view name, HttpMethod, const Operation&);
    virtual void leave_operation(std::string_view name, HttpMethod, const Operation&);
    virtual void visit_parameter(const Parameter&);
    virtual void visit_response(std::string_view status_or_component_name, const Response&);
    virtual void visit_schema(const Schema&, std::string_view json_pointer);
    virtual void visit_request_body(const RequestBody&);
};

class DocumentWalker {
public:
    static void walk(const Document&, DocumentVisitor&);
};
```

## Traversal coverage and order

The document callbacks enclose these groups, in order:

1. Document `paths`, then `webhooks`.
2. Component `schemas`, `parameters`, `request_bodies`, and `responses`.

Maps iterate in key order, vectors in index order, and operation maps in `HttpMethod` enum order. For each path item, the walker enters the item, visits its shared parameters, visits its operations, and leaves the item. Within an operation it visits parameters, the request body, and responses before leaving the operation. Parameters include both their direct schema and schemas in their `content` map.

Each schema callback precedes traversal of `properties`, `$defs`, `items`, `not`, `allOf`, `anyOf`, and `oneOf`, in that order. Nested schemas within raw JSON are not visited: this includes `additional_properties`, unknown schema vocabularies, callbacks, response headers/links, and unmodeled component groups.

Path-item and operation callbacks receive the original path key or webhook name, preserving the existing callback interface. A component response callback receives its component name in place of a response status. Schema callbacks receive a complete escaped JSON Pointer, so callers can distinguish paths, webhooks, and components.

## Pointers and cycles

A query parameter's first schema under `/pets/{id}` has a pointer such as:

```text
/paths/~1pets~1{id}/get/parameters/0/schema
```

Parameter arrays use numeric indices. All map-key tokens escape `~` as `~0` and `/` as `~1`; for example, a media type becomes `application~1json`. These are JSON Pointers, not URI fragments, so they have no leading `#` and are not percent-encoded.

Both walkers track the active chain of schemas. They invoke the schema callback for a cycle's closing edge, then stop descending that edge. A shared subtree reached through a separate path is visited again, including its descendants. This gives callbacks a consistent path for every occurrence without looping on recursive pointers.

For a component `Node` whose `properties["self"]` points to itself, callbacks occur at:

```text
/components/schemas/Node
/components/schemas/Node/properties/self
```

This traversal does not resolve `$ref` strings. Use the reference resolver separately when expansion is required.

Each walk permits up to 100,000 schema visits and 256 active schema levels. Exceeding either bound throws `std::runtime_error`; this also bounds work for shared acyclic graphs whose expansion would otherwise grow exponentially. Callbacks completed before the exception retain their effects.

## Mutable traversal

`MutableDocumentVisitor` exposes the same callbacks using mutable references. Its schema callback takes `SchemaPtr&`, allowing the visitor to replace or clear a schema pointer:

```cpp
struct RemoveDeprecatedSchemas : swaggercpp::MutableDocumentVisitor {
    void visit_schema(swaggercpp::SchemaPtr& schema, std::string_view) override {
        if (schema && schema->deprecated) {
            schema.reset();
        }
    }
};

RemoveDeprecatedSchemas visitor;
swaggercpp::MutableDocumentWalker::walk(document, visitor);
```

The walker checks the replacement pointer before descending, including whether it points to an active ancestor. Shared nodes can receive callbacks more than once, so mutable callbacks should account for repeated visits. Avoid erasing or replacing an enclosing container while the walker is iterating it.

## Example: counting operations

```cpp
struct MethodCounter : swaggercpp::DocumentVisitor {
    std::map<swaggercpp::HttpMethod, std::size_t> counts;

    void enter_operation(std::string_view,
                         swaggercpp::HttpMethod method,
                         const swaggercpp::Operation&) override {
        ++counts[method];
    }
};

MethodCounter visitor;
swaggercpp::DocumentWalker::walk(document, visitor);
```

This includes operations on typed webhooks as well as ordinary paths.

## See also

- [Traverse example](/examples/traverse-document)
- [Endpoint catalog example](/examples/endpoint-catalog)
- [Document model](/api/document)
