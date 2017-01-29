#pragma once

#include "format.h"

#include <GL/gl.h>

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
            default:
                return 0;
        }
    }
}

