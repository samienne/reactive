#include "reactive/window.h"

#include "widgetfactory.h"
#include "rendering.h"

namespace reactive
{

Window::Window(WidgetFactory widget,
        Signal<std::string> const& title) :
    widget_(std::move(widget)),
    title_(title)
{
}

Window Window::onClose(std::function<void()> const& cb) &&
{
    closeCallbacks_.push_back(cb);
    return std::move(*this);
}

WidgetFactory Window::getWidget() const
{
    return widget_->clone()
        | background()
        ;
}

Signal<std::string> const& Window::getTitle() const
{
    return title_;
}

void Window::invokeOnClose() const
{
    auto cbs = closeCallbacks_;
    for (auto const& cb : cbs)
        cb();
}

auto window(Signal<std::string> const& title, WidgetFactory widget)
    -> Window
{
    return Window(std::move(widget), title);
}

} // namespace

