#include "bqui/window.h"

#include "bqui/modifier/background.h"

#include "windowlist.h"

#include <mutex>
#include <utility>

namespace bqui
{

struct WindowHandle::Impl
{
    btl::UniqueId const id = btl::makeUniqueId();

    // Guards the list alone. It is taken to read or write the reference and
    // released before the list is touched, so a close() never holds it while
    // the list runs a window's destructor.
    std::mutex mutex;
    std::weak_ptr<WindowList> list;
};

WindowHandle::WindowHandle() :
    impl_(std::make_shared<Impl>())
{
}

WindowHandle::~WindowHandle() = default;

btl::UniqueId WindowHandle::getId() const
{
    return impl_->id;
}

void WindowHandle::close() const
{
    std::shared_ptr<WindowList> list;

    {
        std::lock_guard<std::mutex> lock(impl_->mutex);
        list = impl_->list.lock();
    }

    if (list)
        list->remove(impl_->id);
}

bool WindowHandle::hasList() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    return impl_->list.lock() != nullptr;
}

void WindowHandle::setList(std::weak_ptr<WindowList> list) const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->list = std::move(list);
}

void WindowHandle::clearList() const
{
    std::lock_guard<std::mutex> lock(impl_->mutex);
    impl_->list.reset();
}

Window::Window(widget::AnyWidget widget,
        bq::signal::AnySignal<std::string> const& title) :
    widget_(std::move(widget)),
    title_(title.share())
{
}

Window::Window(widget::AnyWidget widget,
        bq::signal::AnySignal<std::string> const& title,
        WindowHandle handle) :
    widget_(std::move(widget)),
    title_(title.share()),
    handle_(std::move(handle))
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
        | modifier::background()
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

btl::UniqueId Window::getId() const
{
    return handle_.getId();
}

WindowHandle const& Window::getHandle() const
{
    return handle_;
}

void Window::close() const
{
    handle_.close();
}

auto window(bq::signal::AnySignal<std::string> const& title, widget::AnyWidget widget)
    -> Window
{
    return Window(std::move(widget), title);
}

auto window(bq::signal::AnySignal<std::string> const& title,
        widget::AnyWidget widget, WindowHandle handle) -> Window
{
    return Window(std::move(widget), title, std::move(handle));
}

}

