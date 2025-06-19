#include "window.h"

#include "widget/background.h"

namespace reactive
{

Window::Window(widget::AnyWidget widget,
        bq::signal::AnySignal<std::string> const& title) :
    widget_(std::move(widget)),
    title_(title.share())
{
}

Window Window::onClose(std::function<void()> const& cb) &&
{
    closeCallbacks_.push_back(cb);
    return std::move(*this);
}

widget::AnyWidget Window::getWidget() const
{
    return widget_.clone()
        | widget::background()
        ;
}

bq::signal::AnySignal<std::string> const& Window::getTitle() const
{
    return title_;
}

void Window::invokeOnClose() const
{
    auto cbs = closeCallbacks_;
    for (auto const& cb : cbs)
        cb();
}

auto window(bq::signal::AnySignal<std::string> const& title, widget::AnyWidget widget)
    -> Window
{
    return Window(std::move(widget), title);
}

} // namespace

