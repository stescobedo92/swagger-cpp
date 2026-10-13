# httplib can be header-only without a pkg-config module (as in vcpkg).
# Its enabled backends still contribute link dependencies to our static archive.
set(SWAGGERCPP_PC_REQUIRES_PRIVATE "yaml-cpp")
set(SWAGGERCPP_PC_LIBS_PRIVATE "${CMAKE_THREAD_LIBS_INIT}")
set(httplib_definitions "$<TARGET_GENEX_EVAL:httplib::httplib,$<TARGET_PROPERTY:httplib::httplib,INTERFACE_COMPILE_DEFINITIONS>>")
set(httplib_features BROTLI OPENSSL ZLIB ZSTD WOLFSSL MBEDTLS)
set(httplib_modules "libbrotlienc libbrotlidec" openssl zlib libzstd wolfssl mbedtls)
foreach(feature module IN ZIP_LISTS httplib_features httplib_modules)
    string(APPEND SWAGGERCPP_PC_REQUIRES_PRIVATE
        " $<$<IN_LIST:CPPHTTPLIB_${feature}_SUPPORT,${httplib_definitions}>:${module}>")
endforeach()

get_target_property(httplib_type httplib::httplib TYPE)
if(NOT httplib_type STREQUAL "INTERFACE_LIBRARY")
    if(MSVC)
        # pkgconf --msvc-syntax requires -L/-l instead of bare library paths.
        string(APPEND SWAGGERCPP_PC_LIBS_PRIVATE
            " -L\"$<TARGET_LINKER_FILE_DIR:httplib::httplib>\" -l$<PATH:GET_STEM,LAST_ONLY,$<TARGET_LINKER_FILE_NAME:httplib::httplib>>")
    else()
        string(APPEND SWAGGERCPP_PC_LIBS_PRIVATE
            " \"$<TARGET_LINKER_FILE:httplib::httplib>\"")
    endif()
endif()

if(WIN32)
    string(APPEND SWAGGERCPP_PC_LIBS_PRIVATE " -lws2_32 -lcrypt32 -lshell32")
elseif(APPLE)
    # Retain the frameworks selected by httplib's TLS and resolver backends.
    set(httplib_links "$<TARGET_GENEX_EVAL:httplib::httplib,$<TARGET_PROPERTY:httplib::httplib,INTERFACE_LINK_LIBRARIES>>")
    string(APPEND SWAGGERCPP_PC_LIBS_PRIVATE
        " $<JOIN:$<FILTER:${httplib_links},INCLUDE,^-framework>, >")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/swaggercpp.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/swaggercpp.pc.in"
    @ONLY)
file(GENERATE
    OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/pkgconfig/$<CONFIG>/swaggercpp.pc"
    INPUT "${CMAKE_CURRENT_BINARY_DIR}/swaggercpp.pc.in"
    TARGET swaggercpp)
