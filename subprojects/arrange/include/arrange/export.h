#pragma once

// arrange is built as a shared library by default so that its process-wide
// state -- the counter that mints automatic Variable ids -- exists once per
// process. Linked statically into several modules (a library and the
// executable using it), each module gets its own counter and mints colliding
// ids. Define ARRANGE_STATIC when building and using it as a static library.
#if defined(ARRANGE_STATIC)
#define ARRANGE_API
#elif defined(_WIN32)
#if defined(ARRANGE_BUILD)
#define ARRANGE_API __declspec(dllexport)
#else
#define ARRANGE_API __declspec(dllimport)
#endif
#else
#define ARRANGE_API __attribute__((visibility("default")))
#endif
