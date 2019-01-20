#pragma once

#include "usage.h"
#include "namedvertexspec.h"
#include "vertexbufferimpl.h"
#include "aabb.h"
#include "glbuffer.h"

#include <btl/visibility.h>

namespace ase
{
    class GlRenderContext;
    struct Dispatched;

    class BTL_VISIBLE GlVertexBuffer : public VertexBufferImpl
    {
    public:
        explicit GlVertexBuffer(GlRenderContext& context);
        GlVertexBuffer(GlVertexBuffer const& other) = delete;
        GlVertexBuffer(GlVertexBuffer&& other) = delete;
        virtual ~GlVertexBuffer();

        GlVertexBuffer& operator=(GlVertexBuffer const& other) = delete;
        GlVertexBuffer& operator=(GlVertexBuffer&& other) = delete;

        size_t getSize() const;

        void setData(Dispatched, GlFunctions const& gl, Buffer const& buffer,
                Usage usage);

        GlBuffer const& getBuffer() const;

    private:
        size_t size_;
        GlBuffer buffer_;
    };
}

