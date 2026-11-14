#pragma once

#include "shaderinterface.h"

#include "asevisibility.h"

#include <optional>
#include <string>

namespace ase
{
    class ASE_EXPORT ProgramImpl
    {
    public:
        virtual ~ProgramImpl() = default;
        virtual int getUniformLocation(std::string const& name) const = 0;
        virtual int getAttribLocation(std::string const& name) const = 0;
        virtual ShaderInterface const& getInterface() const = 0;
        virtual std::optional<ShaderInterface> reflect() const = 0;
    };
}
