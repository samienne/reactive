#pragma once

#include "stringify.h"

#include <string>
#include <iostream>

namespace ase
{
    template<typename... Args>
    inline void DBG(std::string const& s, Args&&... args)
    {
        std::cout << stringify(s, args...) << std::endl;
    }
}

