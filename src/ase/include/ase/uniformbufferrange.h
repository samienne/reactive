#pragma once

#include "uniformbuffer.h"

#include "asevisibility.h"

namespace ase
{
    struct ASE_EXPORT UniformBufferRange
    {
        size_t offset;
        size_t size;
        UniformBuffer buffer;
    };
} // namespace ase

