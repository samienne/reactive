#pragma once

#include "vector.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT TextureImpl
    {
    public:
        virtual ~TextureImpl() = default;

        virtual Vector2i getSize() const = 0;
    };
}

