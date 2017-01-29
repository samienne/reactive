#pragma once

#include <string>
#include <typeinfo>
#include <cxxabi.h>

namespace btl
{
    template <typename T>
    std::string demangle()
    {
        auto name = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
        std::string r(name);
        free(name);
        return r;
    }
}

