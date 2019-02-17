#pragma once

#include "uniformbuffer.h"

#include <btl/visibility.h>

namespace ase
{
    struct BTL_VISIBLE UniformBufferRange
    {
        size_t offset;
        size_t size;
        UniformBuffer buffer;
    };
} // namespace ase

