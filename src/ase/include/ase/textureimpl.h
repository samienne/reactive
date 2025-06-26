#pragma once

#include "format.h"
#include "vector.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT TextureImpl
    {
    public:
        virtual ~TextureImpl() = default;

        virtual void setSize(Vector2i size) = 0;
        virtual void setFormat(Format format) = 0;
        virtual Vector2i getSize() const = 0;
        virtual Format getFormat() const = 0;
    };
}

