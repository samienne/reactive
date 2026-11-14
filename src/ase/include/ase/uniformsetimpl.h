#pragma once

#include "shaderinterface.h"

#include "asevisibility.h"

namespace ase
{
    struct UniformBufferRange;

    class ASE_EXPORT UniformSetImpl
    {
    public:
        explicit UniformSetImpl(ShaderInterface iface);
        virtual ~UniformSetImpl() = default;

        ShaderInterface const& getInterface() const;

        virtual void bindUniformBufferRange(
                int binding,
                UniformBufferRange const& buffer
                ) = 0;

    private:
        ShaderInterface iface_;
    };
} // namespace ase

