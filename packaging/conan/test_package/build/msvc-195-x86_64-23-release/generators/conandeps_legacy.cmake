message(STATUS "Conan: Using CMakeDeps conandeps_legacy.cmake aggregator via include()")
message(STATUS "Conan: It is recommended to use explicit find_package() per dependency instead")

find_package(swaggercpp)
find_package(nlohmann_json)
find_package(yaml-cpp)

set(CONANDEPS_LEGACY  swaggercpp::swaggercpp  nlohmann_json::nlohmann_json  yaml-cpp::yaml-cpp )