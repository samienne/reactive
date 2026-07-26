#include "bqui/window.h"

#include "windowdata.h"

#include <memory>
#include <utility>

namespace bqui
{

Window::Window(bq::signal::AnySignal<std::string> const& title) :
    data_(std::make_shared<WindowData>(title))
{
}

Window Window::onClose(std::function<void()> const& cb) &&
{
    data_->addCloseCallback(cb);
    return std::move(*this);
}

bq::signal::AnySignal<std::string> const& Window::getTitle() const
{
    return data_->getTitle();
}

void Window::invokeOnClose() const
{
    data_->invokeOnClose();
}

btl::UniqueId Window::getId() const
{
    return data_->getId();
}

void Window::close() const
{
    data_->close();
}

Window window(bq::signal::AnySignal<std::string> const& title)
{
    return Window(title);
}

}
