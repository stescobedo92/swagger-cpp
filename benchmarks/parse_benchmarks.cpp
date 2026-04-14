#include <benchmark/benchmark.h>

#include "swaggercpp/swaggercpp.hpp"

namespace {

constexpr auto kBenchmarkDocument = R"({
  "openapi": "3.1.0",
  "info": {
    "title": "Benchmark API",
    "version": "1.0.0"
  },
  "paths": {
    "/orders": {
      "get": {
        "operationId": "listOrders",
        "responses": {
          "200": {
            "description": "ok",
            "content": {
              "application/json": {
                "schema": {
                  "type": "array",
                  "items": {
                    "$ref": "#/components/schemas/Order"
                  }
                }
              }
            }
          }
        }
      }
    }
  },
  "components": {
    "schemas": {
      "Order": {
        "type": "object",
        "properties": {
          "id": { "type": "string" },
          "total": { "type": "number", "format": "double" }
        }
      }
    }
  }
})";

void BM_ParseDocument(benchmark::State& state) {
    for (auto _ : state) {
        auto result = swaggercpp::DocumentReader::read(kBenchmarkDocument);
        benchmark::DoNotOptimize(result.ok());
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_ParseDocument);

} // namespace

