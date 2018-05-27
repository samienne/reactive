#pragma once

#include <btl/visibility.h>

#include <string>

namespace ase
{
    class BTL_VISIBLE ProgramImpl
    {
    public:
        virtual ~ProgramImpl() = default;
        virtual int getUniformLocation(std::string const& name) const = 0;
        virtual int getAttribLocation(std::string const& name) const = 0;
    };
}

