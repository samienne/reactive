#pragma once

#include <string>

namespace ase
{
    class ProgramImpl
    {
    public:
        virtual ~ProgramImpl() = default;
        virtual int getUniformLocation(std::string const& name) const = 0;
        virtual int getAttribLocation(std::string const& name) const = 0;
    };
}

