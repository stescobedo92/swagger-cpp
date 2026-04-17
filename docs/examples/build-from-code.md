# Build a document from code

Construct a complete OpenAPI 3.1 document without any input file. Useful when your spec is generated from annotations, reflection, or another source of truth.

## Code

```cpp
#include <iostream>
#include <memory>
#include "swaggercpp/swaggercpp.hpp"

int main() {
    using namespace swaggercpp;

    Document doc;
    doc.version  = SpecificationVersion::openapi_3_1;
    doc.openapi  = "3.1.0";
    doc.info.title       = "Petstore";
    doc.info.version     = "2026.04";
    doc.info.description = "Sample generated spec for the swaggercpp tutorial.";
    doc.info.contact = Contact { .name = "API team", .email = "api@example.com" };
    doc.info.license = License { .name = "MIT", .identifier = "MIT" };

    doc.servers.push_back(Server {
        .url = "https://api.example.com",
        .description = "production",
    });

    // Pet schema
    auto pet = std::make_shared<Schema>();
    pet->type = "object";
    pet->required = { "id", "name" };
    pet->properties.emplace("id",   std::make_shared<Schema>(Schema { .type = "string", .format = "uuid" }));
    pet->properties.emplace("name", std::make_shared<Schema>(Schema { .type = "string" }));
    pet->properties.emplace("tag",  std::make_shared<Schema>(Schema { .type = "string", .nullable = true }));

    doc.components.schemas.emplace("Pet", pet);

    // GET /pets/{id}
    Parameter id_param;
    id_param.name = "id";
    id_param.in = ParameterLocation::path;
    id_param.required = true;
    id_param.schema = std::make_shared<Schema>(Schema { .type = "string", .format = "uuid" });

    Response ok;
    ok.description = "The pet.";
    ok.content.emplace("application/json", MediaType { .schema = pet });

    Operation get_pet;
    get_pet.operation_id = "getPet";
    get_pet.tags = { "pets" };
    get_pet.parameters.push_back(id_param);
    get_pet.responses.emplace("200", ok);
    get_pet.responses.emplace("404", Response { .description = "Not found." });

    PathItem item;
    item.operations.emplace(HttpMethod::get, get_pet);
    doc.paths.emplace("/pets/{id}", item);

    // Serialise
    const auto yaml = DocumentWriter::write_yaml(doc);
    if (!yaml) return 1;

    std::cout << yaml.value();
}
```

## Expected output (abridged)

```yaml
openapi: 3.1.0
info:
  title: Petstore
  version: 2026.04
  description: Sample generated spec for the swaggercpp tutorial.
  contact:
    name: API team
    email: api@example.com
  license:
    name: MIT
    identifier: MIT
servers:
  - url: https://api.example.com
    description: production
paths:
  /pets/{id}:
    get:
      operationId: getPet
      tags: [pets]
      parameters:
        - name: id
          in: path
          required: true
          schema:
            type: string
            format: uuid
      responses:
        "200":
          description: The pet.
          content:
            application/json:
              schema:
                $ref: "#/components/schemas/Pet"
        "404":
          description: Not found.
components:
  schemas:
    Pet:
      type: object
      required: [id, name]
      properties:
        id:   { type: string, format: uuid }
        name: { type: string }
        tag:  { type: string, nullable: true }
```

## Notes

- `SchemaPtr` uses `std::shared_ptr<Schema>`, so the same schema referenced from both `components.schemas["Pet"]` and the response is deduplicated naturally — the writer detects the shared pointer and emits a `$ref`.
- Every field is a plain struct member; there are no setters. Use designated initializers for brevity.

## See also

- [Document model](/api/document)
- [DocumentWriter](/api/writer)
- [Mutate a document](/examples/mutate-document)
