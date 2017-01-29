#pragma once

#include "usage.h"

namespace ase
{
    class RenderContext;
    class NamedVertexSpec;
    class Buffer;
    class Aabb;

    class VertexBufferImpl
    {
    public:
        virtual ~VertexBufferImpl() = default;
    };
}

