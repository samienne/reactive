#pragma once

#include "shaderimpl.h"

namespace ase
{
    class DummyShader : public ShaderImpl
    {
    public:
        explicit DummyShader(ShaderStage stage);
        ShaderStage getStage() const override;

    private:
        ShaderStage stage_;
    };
} // namespace ase
