#pragma once

#include "usage.h"

#include <btl/visibility.h>

namespace ase
{
    class RenderContext;
    class NamedVertexSpec;
    class Buffer;
    class Aabb;

    class BTL_VISIBLE VertexBufferImpl
    {
    public:
        virtual ~VertexBufferImpl() = default;
    };
}

