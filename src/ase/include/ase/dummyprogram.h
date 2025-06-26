#pragma once

#include "programimpl.h"

namespace ase
{
    class DummyProgram : public ProgramImpl
    {
    public:
        int getUniformLocation(std::string const& name) const override;
        int getAttribLocation(std::string const& name) const override;
    };
} // namespace ase

