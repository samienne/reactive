#pragma once

#include "format.h"

#include "systemgl.h"

namespace ase
{
    inline GLenum formatToGl(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGB:
                return GL_RGB;
            case FORMAT_SRGBA:
                return GL_RGBA;
            case FORMAT_R8:
                return GL_RED;
            default:
                return 0;
        }
    }

    inline GLenum formatToGlInternal(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGB:
                return GL_SRGB;
            case FORMAT_SRGBA:
                return GL_SRGB_ALPHA;
            case FORMAT_R8:
                return GL_R8;
            default:
                return 0;
        }
    }
}

