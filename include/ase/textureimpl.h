#pragma once

#include "vector.h"

namespace ase
{
    class TextureImpl
    {
    public:
        virtual ~TextureImpl() = default;

        virtual Vector2i getSize() const = 0;
    };
}

