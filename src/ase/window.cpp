#include "window.h"

#include "windowimpl.h"

namespace ase
{

Window::Window()
{
}

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

void Window::clear()
{
    if (d())
        d()->clear();
}

} // namespace

