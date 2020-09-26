#pragma once

#include "format.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT RenderbufferImpl
    {
    public:
        virtual ~RenderbufferImpl() = default;

        virtual Format getFormat() = 0;
        virtual int getWidth() = 0;
        virtual int getHeight() = 0;
    };
} // ase

