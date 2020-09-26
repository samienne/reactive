#pragma once

#include "asevisibility.h"

#include <stdexcept>

namespace ase
{
    enum ASE_EXPORT Format
    {
        FORMAT_UNKNOWN = 0,
        FORMAT_SRGBA,
        FORMAT_SRGB,
        FORMAT_DEPTH16,
        FORMAT_DEPTH24,
        FORMAT_DEPTH32,
        FORMAT_DEPTH32F,
        FORMAT_DEPTH24_STENCIL8
    };

    inline ASE_EXPORT unsigned int getBytes(Format format)
    {
        switch (format)
        {
            case FORMAT_UNKNOWN:
                return 1;
            case FORMAT_SRGBA:
                return 4;
            case FORMAT_SRGB:
                return 3;
            case FORMAT_DEPTH16:
                return 2;
            case FORMAT_DEPTH24:
                return 3;
            case FORMAT_DEPTH32:
            case FORMAT_DEPTH32F:
            case FORMAT_DEPTH24_STENCIL8:
                return 4;
        }

        return 0;
    }

    inline ASE_EXPORT bool isLinear(Format format)
    {
        switch (format)
        {
            case FORMAT_UNKNOWN:
                return false;
            case FORMAT_SRGBA:
                return false;
            case FORMAT_SRGB:
                return false;
            case FORMAT_DEPTH16:
            case FORMAT_DEPTH24:
            case FORMAT_DEPTH32:
            case FORMAT_DEPTH32F:
            case FORMAT_DEPTH24_STENCIL8:
                return true;
        }

        return 0;
    }
}

