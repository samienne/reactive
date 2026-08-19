#include "platform.h"

#include "window.h"
#include "rendercontext.h"
#include "platformimpl.h"

#include <utility>

namespace ase
{

PauseToken::PauseToken(std::shared_ptr<PlatformImpl> platform) :
    platform_(std::move(platform))
{
}

PauseToken::PauseToken(PauseToken&& other) noexcept :
    platform_(std::move(other.platform_))
{
    other.platform_ = nullptr;
}

PauseToken& PauseToken::operator=(PauseToken&& other) noexcept
{
    if (this != &other)
    {
        if (platform_)
            platform_->resumeFrames();

        platform_ = std::move(other.platform_);
        other.platform_ = nullptr;
    }

    return *this;
}

PauseToken::~PauseToken()
{
    if (platform_)
        platform_->resumeFrames();
}

bool PauseToken::step(std::chrono::microseconds dt)
{
    return platform_ ? platform_->stepFrame(dt) : false;
}

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

RenderContext Platform::makeRenderContext()
{
    return d()->makeRenderContext();
}

btl::RunLoop& Platform::runLoop()
{
    return d()->runLoop();
}

void Platform::run(std::function<bool(Frame const&)> frameCallback)
{
    d()->run(std::move(frameCallback));
}

PauseToken Platform::pause()
{
    d()->pauseFrames();
    return PauseToken(deferred_);
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

