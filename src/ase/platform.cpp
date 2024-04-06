#include "platform.h"

#include "window.h"
#include "rendercontext.h"
#include "platformimpl.h"

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

void Platform::handleEvents()
{
    d()->handleEvents();
}

RenderContext Platform::makeRenderContext()
{
    return d()->makeRenderContext();
}

void Platform::run()
{
    d()->run();
}

} // namespace platform

