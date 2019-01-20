#pragma once

#include "glbuffer.h"

#include "buffer.h"
#include "usage.h"
#include "uniformbufferimpl.h"

namespace ase
{
    class GlRenderContext;
    struct Dispatched;

    class GlUniformBuffer : public UniformBufferImpl
    {
    public:
        explicit GlUniformBuffer(GlRenderContext& context);
        GlUniformBuffer(GlUniformBuffer const&) = delete;
        GlUniformBuffer(GlUniformBuffer&&) = delete;

        GlUniformBuffer& operator=(GlUniformBuffer const&) = delete;
        GlUniformBuffer& operator=(GlUniformBuffer&&) = delete;

        size_t getSize();

        void setData(Dispatched, GlFunctions const& gl, Buffer buffer,
                Usage usage);

        GlBuffer const getBuffer() const;

    private:
        size_t size_;
        GlBuffer buffer_;
    };
} // ase

