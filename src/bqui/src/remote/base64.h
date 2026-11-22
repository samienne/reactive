#pragma once

#include "bqui/bquivisibility.h"

#include <cstddef>
#include <string>

namespace bqui::remote
{
    /**
     * @brief Encode `size` bytes at `data` as base64 (RFC 4648), with padding.
     *
     * `base64Encode(text.data(), text.size())` turns `"foo"` into `"Zm9v"`.
     */
    BQUI_EXPORT std::string base64Encode(void const* data, size_t size);
} // namespace bqui::remote
