# Command-line tool

Build the optional CLI alongside the library:

```sh
cmake -S . -B build -DSWAGGERCPP_BUILD_CLI=ON
cmake --build build
cmake --install build --prefix install
```

Use the same compiler and dependency toolchain as the library. The installed executable is `bin/swaggercpp-cli` (`.exe` on Windows). The option defaults to `OFF` for library consumers.

```sh
swaggercpp-cli validate openapi.yaml
swaggercpp-cli validate openapi.yaml --json --strict --config rules.json
swaggercpp-cli convert openapi.yaml --format json --output openapi.json
swaggercpp-cli bundle openapi.yaml --root . --format yaml --output bundled.yaml
swaggercpp-cli serve openapi.yaml --port 8080 --no-browser
```

`convert` changes JSON/YAML encoding; it does not migrate Swagger/OpenAPI versions. `bundle` resolves supported local file references and preserves recursive references. See [reference support and limits](/api/references). The default bundle format is JSON. Omit `--output` to write the result to standard output. File output is staged beside the destination and then replaced; failed serialization or writing does not truncate the existing destination.

`serve` binds to loopback and prints the UI URL. The default port is dynamically assigned. Stop the process with Ctrl+C. Swagger UI assets currently require an internet connection.

## Validation and CI

`validate` runs the reader and [document validator](/api/validator). It does not fetch external references; bundle multi-file contracts first. `--strict` makes warnings fail the command. The JSON report has an `ok` Boolean and an `issues` array containing `severity`, `ruleId`, `path`, `message`, `file`, and `line`/`column` when known. Source positions are one-based; a missing field points to its containing object.

```json
{
  "requireOperationId": true,
  "requireOperationDescription": true,
  "requireErrorResponse": false,
  "rules": {
    "operation.id.required": { "severity": "error" },
    "operation.description.required": { "enabled": false }
  }
}
```

Configuration uses JSON, is limited to 1 MiB and 16 nesting levels, and rejects unknown rule identifiers, setting names, or incorrect value types. Rule identifiers are listed in the validator reference and available from `DocumentValidator::builtin_rule_ids()`. Custom C++ callbacks are available through the library API, not configuration files.

| Exit code | Meaning |
| --- | --- |
| `0` | Success |
| `1` | Reading, parsing, bundling, serving, or writing failed |
| `2` | Validation failed, or strict mode encountered warnings |
| `64` | Invalid command arguments or configuration |

No files are created by `validate`. JSON validation reports go to standard output; text diagnostics and usage errors normally go to standard error. `--help` lists supported arguments and `--version` prints the package version.
