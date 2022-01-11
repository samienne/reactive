#include "window.h"

#include "widget/drawbackground.h"
#include "widget/builder.h"
#include "widget/buildermodifier.h"

namespace reactive
{

Window::Window(widget::AnyBuilder widget,
        AnySignal<std::string> const& title) :
    widget_(std::move(widget)),
    title_(signal::share(btl::clone(title)))
{
}

Window Window::onClose(std::function<void()> const& cb) &&
{
    closeCallbacks_.push_back(cb);
    return std::move(*this);
}

widget::AnyBuilder Window::getWidget() const
{
    return widget_->clone()
        | widget::background()
        ;
}

AnySharedSignal<std::string> const& Window::getTitle() const
{
    return title_;
}

void Window::invokeOnClose() const
{
    auto cbs = closeCallbacks_;
    for (auto const& cb : cbs)
        cb();
}

auto window(AnySignal<std::string> const& title, widget::AnyBuilder widget)
    -> Window
{
    return Window(std::move(widget), title);
}

} // namespace

