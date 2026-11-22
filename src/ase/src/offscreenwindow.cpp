#include "offscreenwindow.h"

#include "platformbase.h"
#include "rendercontext.h"

namespace ase
{

OffscreenWindow::OffscreenWindow(PlatformBase& platform,
        RenderContext& context, Vector2i size) :
    WindowBase(context, size, 1.0f),
    platform_(platform),
    texture_(context.makeTexture(size, FORMAT_SRGBA)),
    depthbuffer_(context.makeRenderbuffer(size, FORMAT_DEPTH16)),
    framebuffer_(context.makeFramebuffer())
{
    framebuffer_.setColorTarget(0, texture_);
    framebuffer_.setDepthTarget(depthbuffer_);
}

void OffscreenWindow::setVisible(bool /*value*/)
{
}

bool OffscreenWindow::isVisible() const
{
    return false;
}

void OffscreenWindow::setTitle(std::string&& title)
{
    genericWindow_.setTitle(std::move(title));
}

std::string const& OffscreenWindow::getTitle() const
{
    return genericWindow_.getTitle();
}

Framebuffer& OffscreenWindow::getDefaultFramebuffer()
{
    return framebuffer_;
}

bool OffscreenWindow::ownsDefaultFramebuffer() const
{
    return true;
}

void OffscreenWindow::requestFrame()
{
    genericWindow_.requestFrame();
    platform_.requestFrame();
}

std::optional<std::chrono::microseconds> OffscreenWindow::frame(
        Frame const& frame)
{
    return genericWindow_.frame(frame);
}

PresentStatus OffscreenWindow::present()
{
    return PresentStatus::Ok;
}

} // namespace ase
