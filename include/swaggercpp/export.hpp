#pragma once

#if defined(_WIN32) && defined(SWAGGERCPP_SHARED)
#    if defined(SWAGGERCPP_BUILDING_LIBRARY)
#        define SWAGGERCPP_EXPORT __declspec(dllexport)
#    else
#        define SWAGGERCPP_EXPORT __declspec(dllimport)
#    endif
#else
#    define SWAGGERCPP_EXPORT
#endif

