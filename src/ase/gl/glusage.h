#pragma once

#include "usage.h"

#include <GL/gl.h>

namespace ase
{
    inline GLenum usageToGl(Usage usage)
    {
        switch (usage)
        {
            case Usage::StreamDraw:
                return GL_STREAM_DRAW;
            case Usage::StreamRead:
                return GL_STREAM_READ;
            case Usage::StreamCopy:
                return GL_STREAM_COPY;
            case Usage::StaticDraw:
                return GL_STATIC_DRAW;
            case Usage::StaticRead:
                return GL_STATIC_READ;
            case Usage::StaticCopy:
                return GL_STATIC_COPY;
            case Usage::DynamicDraw:
                return GL_DYNAMIC_DRAW;
            case Usage::DynamicRead:
                return GL_DYNAMIC_READ;
            case Usage::DynamicCopy:
                return GL_DYNAMIC_COPY;
            default:
                return 0;
        }
    }
}

