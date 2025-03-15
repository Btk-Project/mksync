//
// core_library.h
//
// Package: mksync
// Library: MksyncCore
// Module:  Core
//

#ifndef _MKSYNC_CORE_LIBRARY_H_
#define _MKSYNC_CORE_LIBRARY_H_

#if defined(_WIN32) && defined(MKS_CORE_DLL)
    #if defined(MKS_CORE_EXPORTS)
        #define MKS_CORE_API __declspec(dllexport)
    #else
        #define MKS_CORE_API __declspec(dllimport)
    #endif
#endif

#if !defined(MKS_CORE_API)
    #if !defined(MKS_NO_GCC_API_ATTRIBUTE) && defined(__GNUC__) && (__GNUC__ >= 4)
        #define MKS_CORE_API __attribute__((visibility("default")))
    #else
        #define MKS_CORE_API
    #endif
#endif

#define MKS_CORE_BEGIN                                                                             \
    namespace core                                                                                 \
    {
#define MKS_CORE_END }

#endif
