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
    PlatformBase(loop)
{
}

Window DummyPlatform::makeWindow(RenderContext& context, Vector2i size,
        bool headless)
{
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

void DummyPlatform::setMaxFrames(uint64_t maxFrames)
{
    maxFrames_ = maxFrames;
}

PlatformBase::RunConfig DummyPlatform::runConfig()
{
    RunConfig config;
    config.frameStep = std::chrono::microseconds(16667);
    config.maxFrames = maxFrames_;

    return config;
}

std::vector<std::weak_ptr<WindowBase>>& DummyPlatform::getRenderWindows()
{
    return renderWindows_;
}

} // namespace ase

