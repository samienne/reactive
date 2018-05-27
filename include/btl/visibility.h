#pragma once

#define BTL_CLASS_VISIBLE __attribute__ ((visibility ("default")))
#define BTL_CLASS_HIDDEN __attribute__ ((visibility ("hidden")))
#define BTL_VISIBLE __attribute__ ((visibility ("default")))
#define BTL_HIDDEN __attribute__ ((visibility ("hidden")))

#define BTL_VISIBILITY_PUSH_HIDDEN _Pragma("GCC visibility push(hidden)")
#define BTL_VISIBILITY_POP _Pragma("GCC visibility pop")

#define BTL_FORCE_INLINE inline __attribute__((always_inline))

