#pragma once

#include <btl/visibility.h>

#ifdef ASE_EXPORT_SYMBOLS
#define ASE_EXPORT BTL_EXPORT
#else
#define ASE_EXPORT BTL_IMPORT
#endif

