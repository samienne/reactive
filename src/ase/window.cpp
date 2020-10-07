#include "window.h"

#include "windowimpl.h"

namespace ase
{

Window::Window(std::shared_ptr<WindowImpl>&& impl) :
    deferred_(std::move(impl))
{
}

Window::~Window()
{
}

void Window::setVisible(bool value)
{
    if (d())
        d()->setVisible(value);
}

bool Window::isVisible() const
{
    if (!d())
        return false;

    return d()->isVisible();
}

void Window::setTitle(std::string title)
{
    if (d())
        d()->setTitle(std::move(title));
}

std::string const& Window::getTitle() const
{
    if (!d())
    {
        static std::string s("");
        return s;
    }

    return d()->getTitle();
}

Vector2i Window::getSize() const
{
    return d()->getSize();
}

Framebuffer& Window::getDefaultFramebuffer()
{
    return d()->getDefaultFramebuffer();
}

void Window::clear()
{
    if (d())
        d()->clear();
}

void Window::setCloseCallback(std::function<void()> func)
{
    d()->setCloseCallback(std::move(func));
}

void Window::setResizeCallback(std::function<void()> func)
{
    d()->setResizeCallback(std::move(func));
}

void Window::setRedrawCallback(std::function<void()> func)
{
    d()->setRedrawCallback(std::move(func));
}

void Window::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> cb)
{
    d()->setButtonCallback(std::move(cb));
}

void Window::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    d()->setPointerCallback(std::move(cb));
}

void Window::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    d()->setDragCallback(std::move(cb));
}

void Window::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    d()->setKeyCallback(std::move(cb));
}

void Window::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    d()->setHoverCallback(std::move(cb));
}

} // namespace

