#pragma once

#if defined(_MSC_VER)
    #if defined(ZSWAGCL_BUILD)
        #define ZSWAGCL_EXPORT __declspec(dllexport)
    #else
        #define ZSWAGCL_EXPORT __declspec(dllimport)
    #endif
#else
    #define ZSWAGCL_EXPORT __attribute__ ((visibility ("default")))
#endif
