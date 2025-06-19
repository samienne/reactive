#pragma once

#include <btl/visibility.h>

#ifdef BQ_EXPORT_SYMBOLS
#define BQ_EXPORT BTL_EXPORT
#define BQ_EXPORT_TEMPLATE BTL_EXPORT_TEMPLATE
#else
#define BQ_EXPORT
#define BQ_EXPORT_TEMPLATE BTL_IMPORT_TEMPLATE
#endif


