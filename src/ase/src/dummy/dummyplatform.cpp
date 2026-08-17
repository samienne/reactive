#include "dummyplatform.h"

#include "dummywindow.h"
#include "dummyrendercontext.h"
#include "offscreenwindow.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"
#include "session.h"

namespace ase
{

Platform makeDummyPlatform(btl::RunLoop& loop)
{
    return Platform(std::make_shared<DummyPlatform>(loop));
}

DummyPlatform::DummyPlatform(btl::RunLoop& loop) :
    PlatformImpl(loop)
{
}

Window DummyPlatform::makeWindow(RenderContext& context, Vector2i size,
        bool headless)
{
    // The dummy loop does not tick windows, so neither kind is auto-driven; the
    // offscreen branch exists so the headless path is uniform across backends.
    if (headless)
        return Window(std::make_shared<OffscreenWindow>(context, size));

    return Window(std::make_shared<DummyWindow>(size));
}

void DummyPlatform::handleEvents()
{
}

RenderContext DummyPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<DummyRenderContext>());
}

void DummyPlatform::setMaxFps(unsigned int fps)
{
    maxFps_ = fps;
}

void DummyPlatform::setMaxFrames(uint64_t maxFrames)
{
    maxFrames_ = maxFrames;
}

Session DummyPlatform::makeSession(RenderContext& context)
{
    // The dummy backend registers no drawable surface (the dummy render queue
    // completes each frame's fence at once). With no OS event source to block
    // on, maxFrames and maxFps stand in: maxFrames self-paces a bounded headless
    // run and maxFps caps it; both zero leaves the loop on-demand, woken by
    // requestFrame().
    Session::Config config;
    config.handleEvents = [this] { handleEvents(); };
    config.frameStep = std::chrono::microseconds(16667);
    config.maxFrames = maxFrames_;
    config.maxFps = maxFps_;

    return Session(*this, context, renderWindows_, std::move(config));
}

} // namespace ase

