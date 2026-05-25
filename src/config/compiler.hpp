#pragma once
#include <version>

// C++ library
#ifdef _MSVC_STL_VERSION
    // msvc stl
    #define MKS_STL_MSVC
    #ifndef _CRT_SECURE_NO_WARNINGS
        #define _CRT_SECURE_NO_WARNINGS
    #endif
#elif defined(_LIBCPP_VERSION)
    // libc++
    #define MKS_STL_LIBCXX
#elif defined(__GLIBCXX__)
    // libstdc++
    #define MKS_STL_STDCXX
#else
    #error "not support other stl"
#endif