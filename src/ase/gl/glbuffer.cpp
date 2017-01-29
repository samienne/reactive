#include "glbuffer.h"

#include "glrendercontext.h"
#include "glplatform.h"
#include "glusage.h"
#include "glerror.h"

#include "debug.h"

namespace ase
{

GlBuffer::GlBuffer(RenderContext& context, GLenum bufferType) :
    platform_(&reinterpret_cast<GlPlatform&>(context.getPlatform())),
    bufferType_(bufferType),
    buffer_(0)
{
    /*context.getImpl<GlRenderContext>().dispatch([this]()
        {
            glGenBuffers(1, &buffer_);
        });

    context.getImpl<GlRenderContext>().wait();*/

    /*if (!buffer_)
        throw std::runtime_error("Unable to create GlBuffer");*/
}

GlBuffer::~GlBuffer()
{
    destroy();
}

void GlBuffer::setData(Dispatched, GlRenderContext& context, void const* data,
        size_t len, Usage usage)
{
    if (!platform_)
    {
        throw std::runtime_error("Trying to set data to uninitialized "
                "gl buffer");
    }

    GlFunctions const& gl = context.getGlFunctions();

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
    if (platform_ && buffer_)
    {
        GLuint buffer = buffer_;
        buffer_ = 0;
        auto& context = platform_->getDefaultContext()
            .getImpl<GlRenderContext>();
        platform_->dispatchBackground([&context, buffer]()
                {
                    context.getGlFunctions().glDeleteBuffers(1, &buffer);
                    //DBG("Deleted GlBuffer %1", buffer);
                });
    }
}

} // namespace

