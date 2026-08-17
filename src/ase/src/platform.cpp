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

Window Platform::makeWindow(RenderContext& context, Vector2i size, bool headless)
{
    return d()->makeWindow(context, size, headless);
}

void Platform::handleEvents()
{
    d()->handleEvents();
}

RenderContext Platform::makeRenderContext()
{
    return d()->makeRenderContext();
}

btl::RunLoop& Platform::runLoop()
{
    return d()->runLoop();
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

