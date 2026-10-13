set(REPO_ROOT "${CURRENT_PORT_DIR}/../../../..")
set(SOURCE_PATH "${CURRENT_BUILDTREES_DIR}/src/${PORT}-src")

file(REMOVE_RECURSE "${SOURCE_PATH}")
file(COPY
    "${REPO_ROOT}/CMakeLists.txt"
    "${REPO_ROOT}/LICENSE"
    "${REPO_ROOT}/cmake"
    "${REPO_ROOT}/include"
    "${REPO_ROOT}/src"
    DESTINATION "${SOURCE_PATH}")

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES tools SWAGGERCPP_BUILD_CLI)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DSWAGGERCPP_BUILD_TESTS=OFF
        -DSWAGGERCPP_BUILD_BENCHMARKS=OFF
        -DSWAGGERCPP_BUILD_EXAMPLES=OFF)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME swaggercpp CONFIG_PATH lib/cmake/swaggercpp)
vcpkg_fixup_pkgconfig()

if("tools" IN_LIST FEATURES)
    vcpkg_copy_tools(TOOL_NAMES swaggercpp-cli AUTO_CLEAN)
endif()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static")
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
