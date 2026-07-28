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

    // Drive frames through the run loop so any sources registered on it (a
    // remote socket, a timer) are serviced between frames. Each frame re-posts
    // the next; a false return stops the loop.
    std::function<void(btl::RunLoop::Controller&)> tick =
        [&](btl::RunLoop::Controller& controller)
    {
        if (frameCallback(frame))
            controller.post(tick);
        else
            controller.stop();
    };

    runLoop().post(tick);
    runLoop().run();
}

void DummyPlatform::requestFrame()
{
}

} // namespace ase

