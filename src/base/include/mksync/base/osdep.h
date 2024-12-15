//
// osdep.h
//
// Package: mksync
// Library: MksyncBase
// Module:  Base
//

#ifndef _MKSYNC_BASE_OSDEP_H_
#define _MKSYNC_BASE_OSDEP_H_

#include "mksync/base/compiler.h"

#ifdef _WIN32
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0601 // support Win7
    #endif
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #ifndef UNICODE
        #define UNICODE
    #endif
    #ifndef _UNICODE
        #define _UNICODE
    #endif
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
    #ifndef _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
        #define _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    #endif
#endif

/**
 * Mark a function that executes in coroutine context
 *
 * Functions that execute in coroutine context cannot be called directly from
 * normal functions.  In the future it would be nice to enable compiler or
 * static checker support for catching such errors.  This annotation might make
 * it possible and in the meantime it serves as documentation.
 *
 * For example:
 *
 *   static void coroutine_fn foo(void) {
 *       ....
 *   }
 */
#ifdef __clang__
    #define mks_coroutine_fn MKS_ANNOTATE("coroutine_fn")
#else
    #define mks_coroutine_fn
#endif

/**
 * Mark a function that can suspend when executed in coroutine context,
 * but can handle running in non-coroutine context too.
 */
#ifdef __clang__
    #define mks_coroutine_mixed_fn MKS_ANNOTATE("coroutine_mixed_fn")
#else
    #define mks_coroutine_mixed_fn
#endif

/**
 * Mark a function that should not be called from a coroutine context.
 * Usually there will be an analogous, coroutine_fn function that should
 * be used instead.
 *
 * When the function is also marked as coroutine_mixed_fn, the function should
 * only be called if the caller does not know whether it is in coroutine
 * context.
 *
 * Functions that are only no_coroutine_fn, on the other hand, should not
 * be called from within coroutines at all.  This for example includes
 * functions that block.
 *
 * In the future it would be nice to enable compiler or static checker
 * support for catching such errors.  This annotation is the first step
 * towards this, and in the meantime it serves as documentation.
 *
 * For example:
 *
 *   static void no_coroutine_fn foo(void) {
 *       ....
 *   }
 */
#ifdef __clang__
    #define mks_no_coroutine_fn MKS_ANNOTATE("no_coroutine_fn")
#else
    #define mks_no_coroutine_fn
#endif

#define MKS_BEGIN                                                                                  \
    namespace mks                                                                                  \
    {
#define MKS_END }

#endif