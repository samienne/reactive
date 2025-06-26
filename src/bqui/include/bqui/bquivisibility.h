#pragma once

#include <btl/visibility.h>

#ifdef BQUI_EXPORT_SYMBOLS
#define BQUI_EXPORT BTL_EXPORT
#define BQUI_EXPORT_TEMPLATE BTL_EXPORT_TEMPLATE
#else
#define BQUI_EXPORT
#define BQUI_EXPORT_TEMPLATE BTL_IMPORT_TEMPLATE
#endif

