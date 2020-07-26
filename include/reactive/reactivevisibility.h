#pragma once

#include <btl/visibility.h>

#ifdef REACTIVE_EXPORT_SYMBOLS
#define REACTIVE_EXPORT BTL_EXPORT
#else
#define REACTIVE_EXPORT BTL_IMPORT
#endif

