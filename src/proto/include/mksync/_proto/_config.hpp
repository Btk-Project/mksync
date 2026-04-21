#pragma once
#include <mksync/config.hpp>
// Standard Library
#include <version>

// MKS_PROTO_API
#if defined(_WIN32) && defined(MKS_SHARED_BUILD)
    #if defined(MKS_COROUTINE_COMPILING)
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

// clang-format off
#define MKS_PROTO_LOG_TRACE(...)    SPDLOG_TRACE(__VA_ARGS__)
#define MKS_PROTO_LOG_DEBUG(...)    SPDLOG_DEBUG(__VA_ARGS__)
#define MKS_PROTO_LOG_INFO(...)     SPDLOG_INFO(__VA_ARGS__)
#define MKS_PROTO_LOG_WARN(...)     SPDLOG_WARN(__VA_ARGS__)
#define MKS_PROTO_LOG_ERROR(...)    SPDLOG_ERROR(__VA_ARGS__)
#define MKS_PROTO_LOG_CRITICAL(...) SPDLOG_CRITICAL(__VA_ARGS__)
// clang-format on
