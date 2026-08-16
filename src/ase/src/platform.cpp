#include "platform.h"

#include "window.h"
#include "rendercontext.h"
#include "platformimpl.h"
#include "session.h"

namespace ase
{

Platform::Platform(std::shared_ptr<PlatformImpl> impl) :
    deferred_(std::move(impl))
{
}

Platform::~Platform()
{
}

Window Platform::makeWindow(Vector2i size)
{
    return d()->makeWindow(size);
}

Window Platform::makeOffscreenWindow(RenderContext& context, Vector2i size)
{
    return d()->makeOffscreenWindow(context, size);
}

void Platform::handleEvents()
{
    d()->handleEvents();
}

RenderContext Platform::makeRenderContext()
{
    return d()->makeRenderContext();
}

void Platform::run(RenderContext& renderContext,
        std::function<bool(Frame const&)> frameCallback)
{
    d()->run(renderContext, std::move(frameCallback));
}

Session Platform::makeSession(RenderContext& context)
{
    return d()->makeSession(context);
}

void Platform::requestFrame()
{
    d()->requestFrame();
}

PlatformImpl* Platform::getImplOfType(std::type_index type)
{
    return d()->getImplOfType(type);
}

} // namespace platform

