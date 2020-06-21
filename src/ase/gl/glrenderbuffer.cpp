#include "glrenderbuffer.h"

#include "glrendercontext.h"
#include "glfunctions.h"

namespace ase
{

GlRenderbuffer::GlRenderbuffer(GlRenderContext& context, Vector2i size, Format format) :
    context_(context),
    format_(format),
    glObject_(0),
    size_(size)
{
    assert(format == FORMAT_DEPTH16);

    context_.dispatch([size, this](GlFunctions const& gl)
            {

                gl.glGenRenderbuffers(1, &glObject_);
                gl.glBindRenderbuffer(GL_RENDERBUFFER, glObject_);

                gl.glRenderbufferStorage(
                        GL_RENDERBUFFER,
                        GL_DEPTH_COMPONENT16,
                        size[0],
                        size[1]);
            });

    context_.wait();
}

GLuint GlRenderbuffer::getGlObject() const
{
    return glObject_;
}

Format GlRenderbuffer::getFormat() const
{
    return format_;
}

Vector2i GlRenderbuffer::getSize() const
{
    return size_;
}


} // namespace ase

