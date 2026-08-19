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
    // Register both kinds on the render list so the one platform loop drives
    // their frame() the same way the real backends do; there is no lock (the
    // dummy runs single-threaded on the loop thread) and no event-routing list
    // (the dummy has no OS windows to route events to). The draws themselves are
    // no-ops on the dummy queue.
    if (headless)
    {
        auto window = std::make_shared<OffscreenWindow>(context, size);
        renderWindows_.push_back(window);
        return Window(std::move(window));
    }

    auto window = std::make_shared<DummyWindow>(context, size);
    renderWindows_.push_back(window);
    return Window(std::move(window));
}

void DummyPlatform::handleEvents()
{
}

RenderContext DummyPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<DummyRenderContext>(
                shared_from_this()));
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
    // The dummy backend has no OS event source to block on, so maxFrames and
    // maxFps stand in: maxFrames self-paces a bounded headless run and maxFps
    // caps it; both zero leaves the loop on-demand, woken by requestFrame().
    RunConfig config;
    config.frameStep = std::chrono::microseconds(16667);
    config.maxFrames = maxFrames_;
    config.maxFps = maxFps_;

    return config;
}

std::vector<std::weak_ptr<WindowBase>>& DummyPlatform::getRenderWindows()
{
    return renderWindows_;
}

} // namespace ase

