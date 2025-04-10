//
// base_library.h
//
// Package: mksync
// Library: MksyncBase
// Module:  Base
//

#ifndef _MKSYNC_BASE_LIBRARY_H_
#define _MKSYNC_BASE_LIBRARY_H_
#include <mksync/config.h>

#if defined(_WIN32) && defined(MKS_BASE_DLL)
    #if defined(MKS_BASE_EXPORTS)
        #define MKS_BASE_API __declspec(dllexport)
    #else
        #define MKS_BASE_API __declspec(dllimport)
    #endif
#endif

#if !defined(MKS_BASE_API)
    #if !defined(MKS_NO_GCC_API_ATTRIBUTE) && defined(__GNUC__) && (__GNUC__ >= 4)
        #define MKS_BASE_API __attribute__((visibility("default")))
    #else
        #define MKS_BASE_API
    #endif
#endif

#endif
