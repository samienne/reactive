#pragma once

#include <btl/visibility.h>

#ifdef AVG_EXPORT_SYMBOLS
#define AVG_EXPORT BTL_EXPORT
#define AVG_EXPORT_TEMPLATE BTL_EXPORT_TEMPLATE
#define AVG_EXPORT_CLASS_TEMPLATE BTL_EXPORT_CLASS_TEMPLATE
/* Deliberately left undefined in a consumer: an explicit instantiation
 * definition of an avg template belongs in avg, and a consumer that writes one
 * should fail to compile rather than re-export the symbol. */
#define AVG_INSTANTIATE_TEMPLATE BTL_INSTANTIATE_TEMPLATE
#else
#define AVG_EXPORT BTL_IMPORT
#define AVG_EXPORT_TEMPLATE BTL_IMPORT_TEMPLATE
#define AVG_EXPORT_CLASS_TEMPLATE
#endif
