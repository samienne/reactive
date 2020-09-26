#pragma once

#include "asevisibility.h"

namespace ase
{
    struct UniformBufferRange;

    class ASE_EXPORT UniformSetImpl
    {
    public:
        virtual ~UniformSetImpl() = default;

        virtual void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) = 0;
    };
} // namespace ase

