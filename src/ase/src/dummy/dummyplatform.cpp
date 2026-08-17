#include "dummyplatform.h"

#include "dummywindow.h"
#include "dummyrendercontext.h"
#include "offscreenwindow.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

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

    return Window(std::make_shared<DummyWindow>(context, size));
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

PlatformImpl::RunConfig DummyPlatform::runConfig()
{
    // The dummy backend registers no drawable surface, so its render list stays
    // empty and no window fence is submitted. With no OS event source to block
    // on, maxFrames and maxFps stand in: maxFrames self-paces a bounded headless
    // run and maxFps caps it; both zero leaves the loop on-demand, woken by
    // requestFrame().
    RunConfig config;
    config.frameStep = std::chrono::microseconds(16667);
    config.maxFrames = maxFrames_;
    config.maxFps = maxFps_;
    config.renderWindows = &renderWindows_;

    return config;
}

} // namespace ase

