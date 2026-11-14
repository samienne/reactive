#pragma once

#include "programimpl.h"
#include "shaderinterface.h"

namespace ase
{
    class DummyProgram : public ProgramImpl
    {
    public:
        explicit DummyProgram(ShaderInterface iface);

        int getUniformLocation(std::string const& name) const override;
        int getAttribLocation(std::string const& name) const override;
        ShaderInterface const& getInterface() const override;
        std::optional<ShaderInterface> reflect() const override;

    private:
        ShaderInterface interface_;
    };
} // namespace ase
