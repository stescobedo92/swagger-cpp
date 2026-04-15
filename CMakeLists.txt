cmake_minimum_required(VERSION 3.26)

project(
    swaggercpp
    VERSION 0.2.1
    DESCRIPTION "Professional C++23 library for reading, writing, validating, and traversing Swagger/OpenAPI documents."
    LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(SWAGGERCPP_BUILD_TESTS "Build unit tests" ON)
option(SWAGGERCPP_BUILD_BENCHMARKS "Build benchmarks" ON)
option(SWAGGERCPP_BUILD_EXAMPLES "Build examples" ON)
option(SWAGGERCPP_ENABLE_WARNINGS "Enable strict warnings" ON)

find_package(nlohmann_json CONFIG REQUIRED)
find_package(yaml-cpp CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

add_library(swaggercpp
    src/document.cpp
    src/detail/json_utils.cpp
    src/detail/yaml_utils.cpp
    src/reader.cpp
    src/ui.cpp
    src/walker.cpp
    src/validator.cpp
    src/writer.cpp)

add_library(swaggercpp::swaggercpp ALIAS swaggercpp)

target_compile_features(swaggercpp PUBLIC cxx_std_23)

target_include_directories(swaggercpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(swaggercpp
    PUBLIC
        nlohmann_json::nlohmann_json
        yaml-cpp::yaml-cpp
    PRIVATE
        httplib::httplib)

target_compile_definitions(swaggercpp
    PRIVATE SWAGGERCPP_BUILDING_LIBRARY
    PUBLIC $<$<BOOL:${BUILD_SHARED_LIBS}>:SWAGGERCPP_SHARED>)

if(SWAGGERCPP_ENABLE_WARNINGS)
    if(MSVC)
        target_compile_options(swaggercpp PRIVATE /W4 /permissive- /EHsc /Zc:preprocessor)
    else()
        target_compile_options(swaggercpp PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    endif()
endif()

set_target_properties(swaggercpp PROPERTIES
    CXX_EXTENSIONS OFF
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME swaggercpp)

install(
    TARGETS swaggercpp
    EXPORT swaggercppTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(
    DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

configure_package_config_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/swaggercppConfig.cmake.in
    ${CMAKE_CURRENT_BINARY_DIR}/swaggercppConfig.cmake
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/swaggercpp)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/swaggercppConfigVersion.cmake
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(
    FILES
        ${CMAKE_CURRENT_BINARY_DIR}/swaggercppConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/swaggercppConfigVersion.cmake
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/swaggercpp)

install(
    EXPORT swaggercppTargets
    FILE swaggercppTargets.cmake
    NAMESPACE swaggercpp::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/swaggercpp)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/cmake/swaggercpp.pc.in
    ${CMAKE_CURRENT_BINARY_DIR}/swaggercpp.pc
    @ONLY)

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/swaggercpp.pc
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)

enable_testing()

if(SWAGGERCPP_BUILD_TESTS)
    find_package(GTest CONFIG REQUIRED)
    include(GoogleTest)

    add_executable(swaggercpp-tests
        tests/reader_tests.cpp
        tests/ui_tests.cpp
        tests/validator_tests.cpp
        tests/writer_tests.cpp)

    target_link_libraries(swaggercpp-tests
        PRIVATE
            swaggercpp::swaggercpp
            httplib::httplib
            GTest::gtest_main)

    target_compile_features(swaggercpp-tests PRIVATE cxx_std_23)
    gtest_discover_tests(swaggercpp-tests)
endif()

if(SWAGGERCPP_BUILD_BENCHMARKS)
    find_package(benchmark CONFIG REQUIRED)

    add_executable(swaggercpp-benchmarks
        benchmarks/parse_benchmarks.cpp)

    target_link_libraries(swaggercpp-benchmarks
        PRIVATE
            swaggercpp::swaggercpp
            benchmark::benchmark
            benchmark::benchmark_main)

    target_compile_features(swaggercpp-benchmarks PRIVATE cxx_std_23)
endif()

if(SWAGGERCPP_BUILD_EXAMPLES)
    foreach(example_name IN ITEMS load_validate build_petstore mutate_document serve_ui)
        add_executable(${example_name} examples/${example_name}.cpp)
        target_link_libraries(${example_name} PRIVATE swaggercpp::swaggercpp)
        target_compile_features(${example_name} PRIVATE cxx_std_23)
    endforeach()
endif()
