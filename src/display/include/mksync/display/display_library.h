//
// display_library.h
//
// Package: mksync
// Library: MksyncDisplay
// Module:  Display
//

#ifndef _MKSYNC_DISPLAY_LIBRARY_H_
#define _MKSYNC_DISPLAY_LIBRARY_H_

// clang-format off
#if defined(_WIN32) && defined(MKS_DLL)
	#if defined(MKS_DISPLAY_EXPORTS)
		#define MKS_DISPLAY_API __declspec(dllexport)
	#else
		#define MKS_DISPLAY_API __declspec(dllimport)
	#endif
#endif

#if !defined(MKS_DISPLAY_API)
	#if !defined(MKS_NO_GCC_API_ATTRIBUTE) && defined (__GNUC__) && (__GNUC__ >= 4)
		#define MKS_DISPLAY_API __attribute__((visibility ("default")))
	#else
		#define MKS_DISPLAY_API
	#endif
#endif
// clang-format on

#define MKS_DISPLAY_BEGIN                                                                          \
    namespace display                                                                              \
    {
#define MKS_DISPLAY_END }

#endif
