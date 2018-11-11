#include "glbuffer.h"

#include "glrendercontext.h"
#include "glplatform.h"
#include "glusage.h"
#include "glerror.h"

#include "debug.h"

namespace ase
{

GlBuffer::GlBuffer(GlRenderContext& context, GLenum bufferType) :
    context_(context),
    bufferType_(bufferType),
    buffer_(0)
{
}

GlBuffer::~GlBuffer()
{
    destroy();
}

void GlBuffer::setData(Dispatched, void const* data, size_t len, Usage usage)
{
    GlFunctions const& gl = context_.getGlFunctions();

    if (!buffer_)
    {
        gl.glGenBuffers(1, &buffer_);

        if (!buffer_)
            throw std::runtime_error("Unable to create GlBuffer");
    }

    gl.glBindBuffer(bufferType_, buffer_);
    gl.glBufferData(bufferType_, len, data, usageToGl(usage));
    gl.glBindBuffer(bufferType_, 0);
}

GLuint GlBuffer::getBuffer() const
{
    return buffer_;
}

void GlBuffer::destroy()
{
    if (buffer_)
    {
        GLuint buffer = buffer_;
        buffer_ = 0;
        GlRenderContext& context = context_;
        context_.dispatchBg([&context, buffer]()
                {
                    context.getGlFunctions().glDeleteBuffers(1, &buffer);
                    //DBG("Deleted GlBuffer %1", buffer);
                });
    }
}

} // namespace ase

