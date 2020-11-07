#pragma once

#include "format.h"
#include "vector.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT RenderbufferImpl
    {
    public:
        virtual ~RenderbufferImpl() = default;

        virtual Format getFormat() const = 0;
        virtual Vector2i getSize() const = 0;
    };
} // ase

