#pragma once

#include "usage.h"

#include "asevisibility.h"

namespace ase
{
    class RenderContext;
    class NamedVertexSpec;
    class Buffer;
    class Aabb;

    class ASE_EXPORT VertexBufferImpl
    {
    public:
        virtual ~VertexBufferImpl() = default;
    };
}

