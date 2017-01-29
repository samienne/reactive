#include "glrendertargetobject.h"

#include "glrendercontext.h"
#include "rendercontext.h"

#include "texture.h"

#include "debug.h"

#include <algorithm>

namespace ase
{

GlRenderTargetObject::GlRenderTargetObject(RenderContext& context) :
    platform_(&reinterpret_cast<GlPlatform&>(context.getPlatform())),
    dirty_(true)
{
}

GlRenderTargetObject::~GlRenderTargetObject()
{
}

void GlRenderTargetObject::setColorTarget(size_t index, Texture& texture)
{
    if (!texture)
    {
        auto i = colorTextures_.find(index);
        if (i != colorTextures_.end())
            colorTextures_.erase(i);
        return;
    }

    colorTextures_[index] = texture;
}

std::shared_ptr<RenderTargetObjectImpl> GlRenderTargetObject::clone() const
{
    return std::make_shared<GlRenderTargetObject>(*this);
}

void GlRenderTargetObject::makeCurrent(Dispatched,
        RenderContextImpl& renderContext) const
{
    auto& glRenderContext = reinterpret_cast<GlRenderContext&>(renderContext);
    GlFramebuffer& framebuffer = glRenderContext.getSharedFramebuffer(
            Dispatched());

    framebuffer.makeCurrent(Dispatched(), glRenderContext);

    for (auto i = colorTextures_.begin(); i != colorTextures_.end(); ++i)
    {
        framebuffer.setColorTarget(Dispatched(), glRenderContext, i->first,
                i->second);
    }

    glRenderContext.setViewport(Dispatched(),
            colorTextures_.begin()->second.getSize());

    if (dirty_)
    {
        glClearColor(0.0, 0.0, 1.0, 1.0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        dirty_ = false;
    }
}

bool GlRenderTargetObject::isComplete() const
{
    return !colorTextures_.empty();
}

void GlRenderTargetObject::clear()
{
    dirty_ = true;
}

Vector2i GlRenderTargetObject::getResolution() const
{
    if (colorTextures_.empty())
        return Vector2i(0, 0);

    int32_t width = 0x7fffffff;
    int32_t height = 0x7fffffff;

    for (auto const& texture : colorTextures_)
    {
        auto size = texture.second.getSize();
        width = std::min(width, size[0]);
        height = std::min(height, size[0]);
    }

    return Vector2i(width, height);
}

} // namespace

