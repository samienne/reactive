#pragma once

#include <btl/visibility.h>

namespace ase
{
    class UniformBufferRange;

    class BTL_VISIBLE UniformSetImpl
    {
    public:
        virtual ~UniformSetImpl() = default;

        virtual void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) = 0;
    };
} // namespace ase
