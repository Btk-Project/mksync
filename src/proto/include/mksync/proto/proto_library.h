//
// proto_library.h
//
// Package: mksync
// Library: MksyncProto
// Module:  Proto
//

#ifndef _MKSYNC_PROTO_LIBRARY_H_
#define _MKSYNC_PROTO_LIBRARY_H_

#if defined(_WIN32) && defined(MKS_PROTO_DLL)
    #if defined(MKS_PROTO_EXPORTS)
        #define MKS_PROTO_API __declspec(dllexport)
    #else
        #define MKS_PROTO_API __declspec(dllimport)
    #endif
#endif

#if !defined(MKS_PROTO_API)
    #if !defined(MKS_NO_GCC_API_ATTRIBUTE) && defined(__GNUC__) && (__GNUC__ >= 4)
        #define MKS_PROTO_API __attribute__((visibility("default")))
    #else
        #define MKS_PROTO_API
    #endif
#endif

#define MKS_PROTO_BEGIN                                                                            \
    namespace proto                                                                                \
    {
#define MKS_PROTO_END }

#endif
