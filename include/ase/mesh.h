#pragma once

#include "vertexbuffer.h"
#include "indexbuffer.h"
#include "aabb.h"

#include <btl/visibility.h>

namespace ase
{
    class BTL_VISIBLE Mesh
    {
    public:
        Mesh();
        Mesh(Aabb&& aabb, VertexBuffer const& vertexBuffer);
        Mesh(Aabb&& aabb, VertexBuffer const& vertexBuffer,
                IndexBuffer const& indexBuffer);
        ~Mesh();

        Aabb const& getAabb() const;
        VertexBuffer const& getVertexBuffer() const;
        IndexBuffer const& getIndexBuffer() const;
        bool hasIndexBuffer() const;

        bool operator<(Mesh const& other) const;

    private:
        Aabb aabb_;
        VertexBuffer vertexBuffer_;
        IndexBuffer indexBuffer_;
    };
}

