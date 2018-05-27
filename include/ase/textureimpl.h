#pragma once

#include "vector.h"

#include <btl/visibility.h>

namespace ase
{
    class BTL_VISIBLE TextureImpl
    {
    public:
        virtual ~TextureImpl() = default;

        virtual Vector2i getSize() const = 0;
    };
}

