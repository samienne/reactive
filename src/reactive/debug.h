#pragma once

#include <ase/stringify.h>

#include <string>
#include <iostream>

namespace reactive
{
    template<typename... Args>
    inline void DBG(std::string const& s, Args&&... args)
    {
        std::cout << ase::stringify(s, args...) << std::endl;
    }
}

