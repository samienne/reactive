#include "dummyplatform.h"

#include "dummywindow.h"
#include "dummyrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

namespace ase
{

Platform makeDefaultPlatform()
{
    return Platform(std::make_shared<DummyPlatform>());
}

Window DummyPlatform::makeWindow(Vector2i /*size*/)
{
    return Window(std::make_shared<DummyWindow>());
}

void DummyPlatform::handleEvents()
{
}

RenderContext DummyPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<DummyRenderContext>());
}

void DummyPlatform::run(RenderContext&,
        std::function<bool(Frame const&)> frameCallback)
{
    Frame frame{};
    while (frameCallback(frame)) {
    }
}

void DummyPlatform::requestFrame()
{
}

} // namespace ase

