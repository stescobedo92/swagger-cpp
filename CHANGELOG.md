# Changelog

## 0.4.0

- Preserve more OpenAPI 3.0/3.1 and JSON Schema fields when reading and writing documents, including Boolean schemas, type unions, precise numeric constraints, definitions, callbacks, webhooks, security requirements, and server overrides.
- Bundle references across local JSON and YAML files, preserving recursive references and enforcing configurable file, size, depth, and allowed-directory limits. Remote references and unsupported schema scopes report diagnostics.
- Configure validation rules by stable identifiers, change their severity, and register custom validators. Optional source tracking includes file, line, and column in diagnostics.
- Add the optional `swaggercpp-cli` executable with `validate`, `convert`, `bundle`, and `serve` commands, JSON diagnostics, rule configuration, and atomic file output.
- Bound parsing and traversal work, detect cycles, and improve malformed-input handling.
- Restore CLI integration coverage and test release artifacts before publishing.
- Package the CLI through the optional vcpkg `tools` feature and publish a portable filesystem registry with verified source hashes and consumer checks.

### Compatibility

The C++ API remains in the 0.x development series. Rebuild consumers against 0.4.0 because public model structures have grown. Validation now reports malformed or unsupported inputs more precisely. The CLI is disabled by default in CMake; enable it with `SWAGGERCPP_BUILD_CLI=ON`.

## 0.3.0

- Add local `$ref` resolution with cycle detection and automated release packaging.
