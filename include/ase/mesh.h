#pragma once

#include "vertexbuffer.h"
#include "indexbuffer.h"
#include "aabb.h"

#include <btl/option.h>
#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT Mesh
    {
    public:
        Mesh(Aabb const& aabb, VertexBuffer vertexBuffer);
        Mesh(Aabb const& aabb, VertexBuffer vertexBuffer,
                IndexBuffer indexBuffer);
        ~Mesh();

        Aabb const& getAabb() const;
        VertexBuffer const& getVertexBuffer() const;
        IndexBuffer const& getIndexBuffer() const;
        bool hasIndexBuffer() const;

        bool operator<(Mesh const& other) const;

    private:
        Aabb aabb_;
        VertexBuffer vertexBuffer_;
        btl::option<IndexBuffer> indexBuffer_;
    };
}

