#pragma once

#include "format.h"

#include <btl/visibility.h>

namespace ase
{
    class BTL_VISIBLE RenderbufferImpl
    {
    public:
        virtual ~RenderbufferImpl() = default;

        virtual Format getFormat() = 0;
        virtual int getWidth() = 0;
        virtual int getHeight() = 0;
    };
} // ase

