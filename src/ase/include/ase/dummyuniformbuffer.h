#pragma once

#include "uniformbufferimpl.h"

namespace ase
{
    class DummyUniformBuffer : public UniformBufferImpl
    {
    public:
        void setData(Buffer buffer, Usage usage) override;
    };
} // namespace ase

