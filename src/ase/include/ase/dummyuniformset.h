#pragma once

#include "uniformsetimpl.h"

namespace ase
{
    class DummyUniformSet : public UniformSetImpl
    {
    public:
        void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) override;
    };
} // namespace ase

