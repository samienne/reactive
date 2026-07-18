#include "dummyplatform.h"

#include "dummywindow.h"
#include "dummyrendercontext.h"

#include "rendercontext.h"
#include "window.h"
#include "platform.h"

namespace ase
{

DummyPlatform::DummyPlatform() = default;

Platform makeDummyPlatform()
{
    return Platform(std::make_shared<DummyPlatform>());
}

Window DummyPlatform::makeWindow(Vector2i size)
{
    auto window = std::make_shared<DummyWindow>(size);
    windows_.push_back(window);
    return Window(std::move(window));
}

void DummyPlatform::handleEvents()
{
}

RenderContext DummyPlatform::makeRenderContext()
{
    return RenderContext(std::make_shared<DummyRenderContext>());
}

void DummyPlatform::setFrameDelta(std::chrono::microseconds dt)
{
    dt_ = dt;
}

void DummyPlatform::setMaxFrames(uint64_t maxFrames)
{
    maxFrames_ = maxFrames;
}

void DummyPlatform::step(Frame const& frame)
{
    // Drop expired windows, then advance the live ones.
    auto it = windows_.begin();
    while (it != windows_.end())
    {
        if (auto window = it->lock())
        {
            window->frame(frame);
            ++it;
        }
        else
        {
            it = windows_.erase(it);
        }
    }
}

void DummyPlatform::run(RenderContext&,
        std::function<bool(Frame const&)> frameCallback)
{
    for (uint64_t i = 0; maxFrames_ == 0 || i < maxFrames_; ++i)
    {
        handleEvents();

        time_ += dt_;
        Frame frame{ time_, dt_ };

        if (!frameCallback(frame))
            break;

        step(frame);
    }
}

void DummyPlatform::requestFrame()
{
}

} // namespace ase
