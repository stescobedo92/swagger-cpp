cmake_minimum_required(VERSION 3.21)
project(swaggercpp_test_package LANGUAGES CXX)

find_package(nlohmann_json CONFIG REQUIRED PATHS "${CMAKE_BINARY_DIR}/generators" NO_DEFAULT_PATH)
find_package(yaml-cpp CONFIG REQUIRED PATHS "${CMAKE_BINARY_DIR}/generators" NO_DEFAULT_PATH)
find_package(swaggercpp CONFIG REQUIRED)

add_executable(swaggercpp_test_package src/example.cpp)
target_link_libraries(swaggercpp_test_package PRIVATE swaggercpp::swaggercpp)
target_compile_features(swaggercpp_test_package PRIVATE cxx_std_23)
