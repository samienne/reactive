#pragma once

#include "shaderstage.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT ShaderImpl
    {
    public:
        virtual ~ShaderImpl() = default;
        virtual ShaderStage getStage() const = 0;
    };
}
