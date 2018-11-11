#pragma once

#include "usage.h"
#include "indexbufferimpl.h"
#include "glbuffer.h"

#include <btl/visibility.h>

#include <cstring>

namespace ase
{
    class GlRenderContext;
    class Buffer;
    struct Dispatched;

    class BTL_VISIBLE GlIndexBuffer : public IndexBufferImpl
    {
    public:
        GlIndexBuffer(GlRenderContext& context);
        GlIndexBuffer(GlIndexBuffer const& other) = delete;
        GlIndexBuffer(GlIndexBuffer&& other) = delete;
        virtual ~GlIndexBuffer();

        GlIndexBuffer& operator=(GlIndexBuffer const& other) = delete;
        GlIndexBuffer& operator=(GlIndexBuffer&& other) = delete;

        size_t getCount() const;

        void setData(Dispatched, Buffer const& buffer, Usage usage);

    private:
        friend class GlRenderContext;
        GlBuffer const& getBuffer() const;

    private:
        size_t size_;
        GlBuffer buffer_;
    };
}

