#pragma once

#include "format.h"

#include "systemgl.h"

namespace ase
{
    inline GLenum formatToGl(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGBA:
            case FORMAT_RGBA16F:
                return GL_RGBA;
            case FORMAT_R8:
                return GL_RED;
            case FORMAT_UNKNOWN:
            case FORMAT_DEPTH16:
            case FORMAT_DEPTH24:
            case FORMAT_DEPTH32:
            case FORMAT_DEPTH32F:
            case FORMAT_DEPTH24_STENCIL8:
                return 0;
        }

        return 0;
    }

    inline GLenum formatToGlInternal(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGBA:
                return GL_SRGB8_ALPHA8;
            case FORMAT_R8:
                return GL_R8;
            case FORMAT_RGBA16F:
                return GL_RGBA16F;
            case FORMAT_DEPTH16:
                return GL_DEPTH_COMPONENT16;
            case FORMAT_DEPTH24:
                return GL_DEPTH_COMPONENT24;
            case FORMAT_DEPTH32:
                return GL_DEPTH_COMPONENT32;
            case FORMAT_DEPTH32F:
                return GL_DEPTH_COMPONENT32F;
            case FORMAT_DEPTH24_STENCIL8:
                return GL_DEPTH24_STENCIL8;
            case FORMAT_UNKNOWN:
                return 0;
        }

        return 0;
    }

    inline GLenum formatToGlType(Format format)
    {
        switch (format)
        {
            case FORMAT_SRGBA:
            case FORMAT_R8:
                return GL_UNSIGNED_BYTE;
            case FORMAT_RGBA16F:
                return GL_HALF_FLOAT;
            case FORMAT_UNKNOWN:
            case FORMAT_DEPTH16:
            case FORMAT_DEPTH24:
            case FORMAT_DEPTH32:
            case FORMAT_DEPTH32F:
            case FORMAT_DEPTH24_STENCIL8:
                return 0;
        }

        return 0;
    }

    inline GLenum formatToGlDepthAttachment(Format format)
    {
        switch (format)
        {
            case FORMAT_DEPTH16:
            case FORMAT_DEPTH24:
            case FORMAT_DEPTH32:
            case FORMAT_DEPTH32F:
                return GL_DEPTH_ATTACHMENT;
            case FORMAT_DEPTH24_STENCIL8:
                return GL_DEPTH_STENCIL_ATTACHMENT;
            case FORMAT_UNKNOWN:
            case FORMAT_SRGBA:
            case FORMAT_R8:
            case FORMAT_RGBA16F:
                return 0;
        }

        return 0;
    }
}
