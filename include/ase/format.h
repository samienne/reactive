#pragma once

#include <btl/visibility.h>

#include <stdexcept>

namespace ase
{
    enum BTL_VISIBLE Format
    {
        FORMAT_UNKNOWN = 0,
        FORMAT_SRGBA,
        FORMAT_SRGB
    };

    inline BTL_VISIBLE unsigned int getBytes(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGBA:
                return 4;
            case FORMAT_SRGB:
                return 3;
            default:
                throw std::runtime_error("Unknown format");
        }
    }

    inline BTL_VISIBLE bool isLinear(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGBA:
                return true;
            case FORMAT_SRGB:
                return true;
            default:
                throw std::runtime_error("Unknown format");
        }
    }
}

