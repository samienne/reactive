#include "rendertargetobject.h"

#include "texture.h"
#include "rendertargetobjectimpl.h"
#include "rendertarget.h"
#include "rendercontext.h"

#include "platform.h"

namespace ase
{

RenderTargetObject::RenderTargetObject(
        std::shared_ptr<RenderTargetObjectImpl> impl) :
    deferred_(std::move(impl))
{
}

RenderTargetObject::~RenderTargetObject()
{
}

void RenderTargetObject::setColorTarget(size_t index,
        btl::option<Texture> texture)
{
    d()->setColorTarget(index, std::move(texture));
}

void RenderTargetObject::clear()
{
    d()->clear();
}

} // namespace

