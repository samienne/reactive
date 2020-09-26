#pragma once

#include <btl/visibility.h>

#ifdef AVG_EXPORT_SYMBOLS
#define AVG_EXPORT BTL_EXPORT
#else
#define AVG_EXPORT BTL_IMPORT
#endif

