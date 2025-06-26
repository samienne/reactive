#include "glbuffer.h"

#include "glrendercontext.h"
#include "glplatform.h"
#include "glusage.h"
#include "glerror.h"
#include "glfunctions.h"

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

void GlBuffer::setData(Dispatched, GlFunctions const& gl, void const* data,
        size_t len, Usage usage)
{
    if (!buffer_)
    {
        gl.glGenBuffers(1, &buffer_);

        context_.validate("glGenBuffers");

        if (!buffer_)
            throw std::runtime_error("Unable to create GlBuffer");
    }

    gl.glBindBuffer(bufferType_, buffer_);
    context_.validate("glBindBuffer");

    gl.glBufferData(bufferType_, len, data, usageToGl(usage));
    context_.validate("glBufferData");
    //gl.glBindBuffer(bufferType_, 0);
}

GLuint GlBuffer::getBuffer() const
{
    return buffer_;
}

GLenum GlBuffer::getBufferType() const
{
    return bufferType_;
}

void GlBuffer::destroy()
{
    if (buffer_)
    {
        GLuint buffer = buffer_;
        buffer_ = 0;

        context_.dispatchBg([buffer, validate=context_.isValidationEnabled()]
                (GlFunctions const& gl)
                {
                    gl.glDeleteBuffers(1, &buffer);
                    //DBG("Deleted GlBuffer %1", buffer);
                    GlRenderContext::validate(validate, "glBindBuffer");
                });
    }
}

} // namespace ase

