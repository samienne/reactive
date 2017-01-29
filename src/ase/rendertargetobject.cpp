#include "rendertargetobject.h"

#include "texture.h"
#include "rendertargetobjectimpl.h"
#include "rendertarget.h"
#include "rendercontext.h"

#include "platform.h"

namespace ase
{

RenderTargetObject::RenderTargetObject()
{
}

RenderTargetObject::RenderTargetObject(RenderContext& context) :
    RenderTarget(context.getPlatform().makeRenderTargetObjectImpl(context))
{
}

RenderTargetObject::~RenderTargetObject()
{
}

void RenderTargetObject::setColorTarget(size_t index, Texture& texture)
{
    if (d())
        d()->setColorTarget(index, texture);
}

void RenderTargetObject::clear()
{
    if (d())
        d()->clear();
}

} // namespace

