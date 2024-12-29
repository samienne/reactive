#pragma once

#include <string>
#include <typeinfo>

#ifdef __unix__
#include <cxxabi.h>
#endif

namespace btl
{
#ifdef __unix__
    template <typename T>
    std::string demangle()
    {
        auto name = abi::__cxa_demangle(typeid(T).name(), 0, 0, 0);
        std::string r(name);
        free(name);
        return r;
    }

    inline std::string demangle(char const* typeName)
    {
        auto name = abi::__cxa_demangle(typeName, 0, 0, 0);
        std::string r(name);
        free(name);
        return r;
    }
#else
    template <typename T>
    std::string demangle()
    {
        return "Unknown";
    }

    inline std::string demangle(char const*)
    {
        return "Unknown";
    }
#endif
}

