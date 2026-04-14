# Architecture Notes

## Design goals

- Keep the public API strongly typed and friendly to tooling authors
- Separate document representation from I/O and validation concerns
- Package the library as a first-class consumable, not just a local build
- Stay explicit about supported spec fidelity

## Main modules

- `document.hpp`: canonical in-memory model
- `reader.cpp`: format detection, parsing, and issue collection
- `writer.cpp`: JSON/YAML serialization and roundtrip helpers
- `validator.cpp`: semantic checks independent from syntax parsing
- `walker.cpp`: visitor traversal across paths, operations, parameters, responses, and schemas
- `detail/json_utils.*` and `detail/yaml_utils.*`: internal translation helpers

## Packaging strategy

- Installable CMake package via exported targets and config files
- Conan recipe for binary/package consumers
- vcpkg overlay port and custom registry assets for private/internal distribution
- GitHub release pipeline for source archives and binary artifacts

