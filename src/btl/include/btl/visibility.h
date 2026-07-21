#pragma once

#if defined(__unix__) || defined(__APPLE__)
    #define BTL_CLASS_VISIBLE __attribute__ ((visibility ("default")))
    #define BTL_CLASS_HIDDEN __attribute__ ((visibility ("hidden")))
    #define BTL_VISIBLE __attribute__ ((visibility ("default")))
    #define BTL_HIDDEN __attribute__ ((visibility ("hidden")))

    #define BTL_VISIBILITY_PUSH_VISIBLE _Pragma("GCC visibility push(default)")
    #define BTL_VISIBILITY_PUSH_HIDDEN _Pragma("GCC visibility push(hidden)")
    #define BTL_VISIBILITY_POP _Pragma("GCC visibility pop")

    #define BTL_FORCE_INLINE inline __attribute__((always_inline))

    #define BTL_EXPORT BTL_VISIBLE
    #define BTL_IMPORT

    /* BTL_EXPORT_TEMPLATE / BTL_IMPORT_TEMPLATE mark the `extern template`
     * declaration in the header; BTL_INSTANTIATE_TEMPLATE marks the explicit
     * instantiation definition in the .cpp. They differ per toolchain because
     * only one of the two places may carry the attribute. */
    #if defined(__clang__)
        #define BTL_EXPORT_TEMPLATE BTL_EXPORT
        #define BTL_INSTANTIATE_TEMPLATE BTL_EXPORT
        #define BTL_EXPORT_CLASS_TEMPLATE
        #define BTL_IMPORT_TEMPLATE BTL_IMPORT
    #elif defined(__GNUC__)
        /* GCC fixes a specialization's visibility where it is first named, so
         * repeating the attribute on the instantiation definition is ignored
         * and diagnosed by -Wattributes. */
        #define BTL_EXPORT_TEMPLATE BTL_EXPORT
        #define BTL_INSTANTIATE_TEMPLATE
        #define BTL_EXPORT_CLASS_TEMPLATE BTL_EXPORT
        #define BTL_IMPORT_TEMPLATE BTL_IMPORT
    #else
        #define BTL_EXPORT_TEMPLATE
        #define BTL_INSTANTIATE_TEMPLATE BTL_EXPORT
        #define BTL_EXPORT_CLASS_TEMPLATE BTL_EXPORT
        #define BTL_IMPORT_TEMPLATE
    #endif

#elif defined(_WIN32)
    #define BTL_EXPORT __declspec(dllexport)
    #define BTL_IMPORT __declspec(dllimport)
    /* dllexport on an `extern template` declaration is C4910 (MSVC) and
     * -Wdllexport-explicit-instantiation-decl (clang-cl): the export belongs on
     * the instantiation definition, which is what the DLL actually emits. */
    #define BTL_EXPORT_TEMPLATE
    #define BTL_INSTANTIATE_TEMPLATE __declspec(dllexport)
    #define BTL_IMPORT_TEMPLATE __declspec(dllimport)
    #define BTL_EXPORT_CLASS_TEMPLATE

    #define BTL_FORCE_INLINE inline
#endif

