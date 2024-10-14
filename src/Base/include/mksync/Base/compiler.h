//
// compiler.h
//
// Package: mksync
// Library: MksyncBase
// Module:  Base
//

#ifndef _MKSYNC_BASE_COMPILER_H_
#define _MKSYNC_BASE_COMPILER_H_

#define MKS_HOST_BIG_ENDIAN (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

/* HOST_LONG_BITS is the size of a native pointer in bits. */
#define MKS_HOST_LONG_BITS (__SIZEOF_POINTER__ * 8)

#if defined __clang_analyzer__ || defined __COVERITY__
#define MKS_STATIC_ANALYSIS 1
#endif

#ifdef __cplusplus
#define MKS_EXTERN_C extern "C"
#define MKS_EXTERN_C_START                                                                                                    \
    extern "C"                                                                                                                 \
    {
#define MKS_EXTERN_C_END }
#else
#define MKS_EXTERN_C extern
#define MKS_EXTERN_C_START
#define MKS_EXTERN_C_END
#endif

#if defined(_WIN32) && !defined(__clang__) && !defined(__MINGW32__)
#define MKS_PACKED
#define MKS_ALIGN8 __declspec(align(8))
#define MKS_ALIGN16 __declspec(align(16))
#define MKS_ALIGN(X) __declspec(align(X))
#else
#define MKS_PACKED __attribute__((packed))
#define MKS_ALIGN8 __attribute__((aligned(8)))
#define MKS_ALIGN16 __attribute__((aligned(16)))
#define MKS_ALIGN(X) __attribute__((aligned(X)))
#endif

// How to packed with msvc:

// #if defined(_WIN32) && !defined(__clang__) && !defined(__MINGW32__)
// #pragma pack(push, 1)
// #endif
// typedef struct A {
//     uint8_t value1;
// } MKS_PACKED A;
// #if defined(_WIN32) && !defined(__clang__) && !defined(__MINGW32__)
// #pragma pack(pop)
// #endif

#ifndef __has_warning
#define __has_warning(x) 0 /* compatibility with non-clang compilers */
#endif

#ifndef __has_feature
#define __has_feature(x) 0 /* compatibility with non-clang compilers */
#endif

#ifndef __has_builtin
#define __has_builtin(x) 0 /* compatibility with non-clang compilers */
#endif

#if __has_builtin(__builtin_assume_aligned) || !defined(__clang__)
#define HAS_ASSUME_ALIGNED
#endif

#ifndef __has_attribute
#define __has_attribute(x) 0 /* compatibility with older GCC */
#endif

#if defined(__SANITIZE_ADDRESS__) || __has_feature(address_sanitizer)
#define MKS_SANITIZE_ADDRESS 1
#endif

#if defined(__SANITIZE_THREAD__) || __has_feature(thread_sanitizer)
#define MKS_SANITIZE_THREAD 1
#endif

/*
 * GCC doesn't provide __has_attribute() until GCC 5, but we know all the GCC
 * versions we support have the "flatten" attribute. Clang may not have the
 * "flatten" attribute but always has __has_attribute() to check for it.
 */
#if __has_attribute(flatten) || !defined(__clang__)
#define MKS_FLATTEN __attribute__((flatten))
#else
#define MKS_FLATTEN
#endif

/*
 * If __attribute__((error)) is present, use it to produce an error at
 * compile time.  Otherwise, one must wait for the linker to diagnose
 * the missing symbol.
 */
#if __has_attribute(error)
#define MKS_ERROR(X) __attribute__((error(X)))
#else
#define MKS_ERROR(X)
#endif

/*
 * The nonstring variable attribute specifies that an object or member
 * declaration with type array of char or pointer to char is intended
 * to store character arrays that do not necessarily contain a terminating
 * NUL character. This is useful in detecting uses of such arrays or pointers
 * with functions that expect NUL-terminated strings, and to avoid warnings
 * when such an array or pointer is used as an argument to a bounded string
 * manipulation function such as strncpy.
 */
#if __has_attribute(nonstring)
#define MKS_NONSTRING __attribute__((nonstring))
#else
#define MKS_NONSTRING
#endif

/*
 * Forced inlining may be desired to encourage constant propagation
 * of function parameters.  However, it can also make debugging harder,
 * so disable it for a non-optimizing build.
 */
#if defined(__OPTIMIZE__)
#define MKS_ALWAYS_INLINE __attribute__((always_inline))
#else
#define MKS_ALWAYS_INLINE
#endif

/**
 * In most cases, normal "fallthrough" comments are good enough for
 * switch-case statements, but sometimes the compiler has problems
 * with those. In that case you can use MKS_FALLTHROUGH instead.
 */
#if __has_attribute(fallthrough)
#define MKS_FALLTHROUGH __attribute__((fallthrough))
#else
#define MKS_FALLTHROUGH                                                                                                       \
    do {                                                                                                                       \
    } while (0) /* fallthrough */
#endif

#if __has_attribute(annotate)
#define MKS_ANNOTATE(x) __attribute__((annotate(x)))
#else
#define MKS_ANNOTATE(x)
#endif

#if __has_attribute(used)
#define MKS_USED __attribute__((used))
#else
#define MKS_USED
#endif

#endif