#pragma once

#include "usage.h"

namespace ase
{
    class Buffer;

    class UniformBufferImpl
    {
    public:
        virtual ~UniformBufferImpl() = default;
        virtual void setData(Buffer buffer, Usage usage) = 0;
    };
} // namespace ase

