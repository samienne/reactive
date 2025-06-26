#pragma once

#include "usage.h"
#include "indexbufferimpl.h"
#include "glbuffer.h"

#include "asevisibility.h"

#include <cstring>

namespace ase
{
    class GlRenderContext;
    class Buffer;
    struct Dispatched;

    class ASE_EXPORT GlIndexBuffer : public IndexBufferImpl
    {
    public:
        GlIndexBuffer(GlRenderContext& context);
        GlIndexBuffer(GlIndexBuffer const& other) = delete;
        GlIndexBuffer(GlIndexBuffer&& other) = delete;
        virtual ~GlIndexBuffer();

        GlIndexBuffer& operator=(GlIndexBuffer const& other) = delete;
        GlIndexBuffer& operator=(GlIndexBuffer&& other) = delete;

        size_t getCount() const;

        void setData(Dispatched, GlFunctions const& gl, Buffer const& buffer,
                Usage usage);

        GlBuffer const& getBuffer() const;

    private:
        size_t size_;
        GlBuffer buffer_;
    };
}

