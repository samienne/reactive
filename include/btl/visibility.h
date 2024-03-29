#pragma once

#if defined(__unix__) || defined(__APPLE__)
    #define BTL_CLASS_VISIBLE __attribute__ ((visibility ("default")))
    #define BTL_CLASS_HIDDEN __attribute__ ((visibility ("hidden")))
    #define BTL_VISIBLE __attribute__ ((visibility ("default")))
    #define BTL_HIDDEN __attribute__ ((visibility ("hidden")))

    #define BTL_VISIBILITY_PUSH_HIDDEN _Pragma("GCC visibility push(hidden)")
    #define BTL_VISIBILITY_POP _Pragma("GCC visibility pop")

    #define BTL_FORCE_INLINE inline __attribute__((always_inline))

    #define BTL_EXPORT BTL_VISIBLE
    #define BTL_IMPORT

    #if defined(__clang__)
        #define BTL_EXPORT_TEMPLATE BTL_EXPORT
        #define BTL_EXPORT_CLASS_TEMPLATE
        #define BTL_IMPORT_TEMPLATE BTL_IMPORT
    #else
        #define BTL_EXPORT_TEMPLATE
        #define BTL_EXPORT_CLASS_TEMPLATE BTL_EXPORT
        #define BTL_IMPORT_TEMPLATE
    #endif

#elif defined(_WIN32)
    #define BTL_EXPORT __declspec(dllexport)
    #define BTL_IMPORT __declspec(dllimport)
    #define BTL_EXPORT_TEMPLATE
    #define BTL_IMPORT_TEMPLATE
    #define BTL_EXPORT_CLASS_TEMPLATE

    #define BTL_FORCE_INLINE inline
#endif

