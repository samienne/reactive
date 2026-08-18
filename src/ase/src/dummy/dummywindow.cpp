#include "dummywindow.h"

#include "dummyframebuffer.h"

#include <memory>

namespace ase
{

DummyWindow::DummyWindow(RenderContext& context, Vector2i size) :
    WindowBase(context, size, 1.0f),
    defaultFramebuffer_(std::make_shared<DummyFramebuffer>())
{
}

void DummyWindow::setVisible(bool value)
{
    visible_ = value;
}

bool DummyWindow::isVisible() const
{
    return visible_;
}

void DummyWindow::setTitle(std::string&& title)
{
    genericWindow_.setTitle(std::move(title));
}

std::string const& DummyWindow::getTitle() const
{
    return genericWindow_.getTitle();
}

Framebuffer& DummyWindow::getDefaultFramebuffer()
{
    return defaultFramebuffer_;
}

void DummyWindow::requestFrame()
{
    genericWindow_.requestFrame();
}

bool DummyWindow::needsRedraw() const
{
    return genericWindow_.needsRedraw();
}

std::optional<std::chrono::microseconds> DummyWindow::frame(Frame const& frame)
{
    return genericWindow_.frame(frame);
}

PresentStatus DummyWindow::present()
{
    return PresentStatus::Ok;
}

} // namespace ase
