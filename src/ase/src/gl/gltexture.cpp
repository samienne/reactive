#include "gltexture.h"

#include "glrendercontext.h"
#include "glplatform.h"
#include "glformat.h"
#include "glfunctions.h"

#include "format.h"
#include "buffer.h"

#include "debug.h"

namespace ase
{

GlTexture::GlTexture(GlRenderContext& context, Vector2i size, Format format) :
    context_(context),
    size_(size),
    texture_(format)
{
}

GlTexture::~GlTexture()
{
    destroy();
}

void GlTexture::setData(Dispatched, GlFunctions const& /*gl*/,
        Vector2i const& size, Format format, Buffer const& buffer)
{
    if (buffer.data()
            && size[0] * size[1] * getBytes(format) > (long)buffer.getSize())
        throw std::runtime_error("GlTexture buffer size is too small.");

    assert(size == size_);
    assert(format_ == format);

    if (!texture_)
        glGenTextures(1, &texture_);

    glBindTexture(GL_TEXTURE_2D, texture_);
    glTexImage2D(GL_TEXTURE_2D, 0, formatToGlInternal(format),
            static_cast<GLsizei>(size[0]), static_cast<GLsizei>(size[1]),
            0, formatToGl(format), GL_UNSIGNED_BYTE, buffer.data());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

    DBG("Loaded texture data %1", texture_);
}

GLuint GlTexture::getGlObject() const
{
    return texture_;
}

void GlTexture::setSize(Vector2i size)
{
    size_ = size;
}

void GlTexture::setFormat(Format format)
{
    format_ = format;
}

Vector2i GlTexture::getSize() const
{
    return size_;
}

Format GlTexture::getFormat() const
{
    return format_;
}

void GlTexture::destroy()
{
    if (texture_)
    {
        GLuint texture = texture_;
        texture_ = 0;

        context_.dispatchBg([texture](GlFunctions const&)
            {
                glDeleteTextures(1, &texture);
                DBG("Texture %1 deleted", texture);
            });
    }
}

} // namespace

