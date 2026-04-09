#pragma once
#include <mksync/config.hpp>

// define MKS_FORCE_EXPORT_API
#if defined(_WIN32)
    #define MKS_FORCE_EXPORT_API __declspec(dllexport)
#endif
#if !defined(MKS_FORCE_EXPORT_API)
    #if defined(__GNUC__) && (__GNUC__ >= 4)
        #define MKS_FORCE_EXPORT_API __attribute__((visibility("default")))
    #else
        #define MKS_FORCE_EXPORT_API
    #endif
#endif
