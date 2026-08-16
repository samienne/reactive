#include "remote/remoteplatform.h"

#include <ase/rendercontext.h>
#include <ase/session.h>
#include <ase/window.h>

#include <algorithm>
#include <memory>
#include <utility>

namespace bqui::remote
{

RemoteWindowImpl::RemoteWindowImpl(ase::Window inner,
        RemotePlatformImpl* owner) :
    inner_(std::move(inner)),
    owner_(owner)
{
}

RemoteWindowImpl::~RemoteWindowImpl()
{
    owner_->unregisterWindow(this);
}

void RemoteWindowImpl::bind(btl::UniqueId id,
        std::function<widget::Introspection()> introspect)
{
    id_ = id;
    introspect_ = std::move(introspect);
}

ase::WindowImpl* RemoteWindowImpl::getImplOfType(std::type_index type)
{
    if (type == std::type_index(typeid(RemoteWindowImpl)))
        return this;

    return inner_.getImplOfType(type);
}

void RemoteWindowImpl::setVisible(bool value)
{
    inner_.setVisible(value);
}

bool RemoteWindowImpl::isVisible() const
{
    return inner_.isVisible();
}

void RemoteWindowImpl::setTitle(std::string&& title)
{
    inner_.setTitle(std::move(title));
}

std::string const& RemoteWindowImpl::getTitle() const
{
    return inner_.getTitle();
}

ase::Vector2i RemoteWindowImpl::getSize() const
{
    return inner_.getSize();
}

float RemoteWindowImpl::getScalingFactor() const
{
    return inner_.getScalingFactor();
}

ase::Framebuffer& RemoteWindowImpl::getDefaultFramebuffer()
{
    return inner_.getDefaultFramebuffer();
}

void RemoteWindowImpl::requestFrame()
{
    inner_.requestFrame();
}

bool RemoteWindowImpl::needsRedraw() const
{
    // False: the session drives this window explicitly, never the polling loop.
    return false;
}

std::optional<std::chrono::microseconds> RemoteWindowImpl::frame(
        ase::Frame const& frame)
{
    if (frameCallback_)
        return frameCallback_(frame);

    return std::nullopt;
}

ase::PresentStatus RemoteWindowImpl::present(ase::Dispatched dispatched)
{
    return inner_.present(dispatched);
}

void RemoteWindowImpl::setFrameCallback(
        std::function<std::optional<std::chrono::microseconds>(
            ase::Frame const&)> func)
{
    // Captured, not forwarded: the inner platform's loop never runs remotely,
    // so advance() is what plays this callback, on the client's clock.
    frameCallback_ = std::move(func);
}

void RemoteWindowImpl::setCloseCallback(std::function<void()> func)
{
    inner_.setCloseCallback(std::move(func));
}

void RemoteWindowImpl::setResizeCallback(std::function<void()> func)
{
    inner_.setResizeCallback(std::move(func));
}

void RemoteWindowImpl::setButtonCallback(
        std::function<void(ase::PointerButtonEvent const&)> cb)
{
    inner_.setButtonCallback(std::move(cb));
}

void RemoteWindowImpl::setPointerCallback(
        std::function<void(ase::PointerMoveEvent const&)> cb)
{
    inner_.setPointerCallback(std::move(cb));
}

void RemoteWindowImpl::setDragCallback(
        std::function<void(ase::PointerDragEvent const&)> cb)
{
    inner_.setDragCallback(std::move(cb));
}

void RemoteWindowImpl::setKeyCallback(
        std::function<void(ase::KeyEvent const&)> cb)
{
    inner_.setKeyCallback(std::move(cb));
}

void RemoteWindowImpl::setHoverCallback(
        std::function<void(ase::HoverEvent const&)> cb)
{
    inner_.setHoverCallback(std::move(cb));
}

void RemoteWindowImpl::setTextCallback(
        std::function<void(ase::TextEvent const&)> cb)
{
    inner_.setTextCallback(std::move(cb));
}

void RemoteWindowImpl::injectPointerButtonEvent(unsigned int pointerIndex,
        unsigned int buttonIndex, ase::Vector2f pos,
        ase::ButtonState buttonState)
{
    inner_.injectPointerButtonEvent(pointerIndex, buttonIndex, pos,
            buttonState);
}

void RemoteWindowImpl::injectPointerMoveEvent(unsigned int pointerIndex,
        ase::Vector2f pos)
{
    inner_.injectPointerMoveEvent(pointerIndex, pos);
}

void RemoteWindowImpl::injectHoverEvent(unsigned int pointerIndex,
        ase::Vector2f pos, bool state)
{
    inner_.injectHoverEvent(pointerIndex, pos, state);
}

void RemoteWindowImpl::injectKeyEvent(ase::KeyState keyState,
        ase::KeyCode keyCode, uint32_t modifiers, std::string text)
{
    inner_.injectKeyEvent(keyState, keyCode, modifiers, std::move(text));
}

void RemoteWindowImpl::injectTextEvent(std::string text)
{
    inner_.injectTextEvent(std::move(text));
}

btl::UniqueId RemoteWindowImpl::id() const
{
    return id_;
}

void RemoteWindowImpl::injectPointerButton(unsigned int pointerIndex,
        unsigned int buttonIndex, ase::Vector2f pos, ase::ButtonState state)
{
    inner_.injectPointerButtonEvent(pointerIndex, buttonIndex, pos, state);
}

void RemoteWindowImpl::injectPointerMove(unsigned int pointerIndex,
        ase::Vector2f pos)
{
    inner_.injectPointerMoveEvent(pointerIndex, pos);
}

void RemoteWindowImpl::injectHover(unsigned int pointerIndex, ase::Vector2f pos,
        bool state)
{
    inner_.injectHoverEvent(pointerIndex, pos, state);
}

void RemoteWindowImpl::injectKey(ase::KeyState state, ase::KeyCode code,
        uint32_t modifiers, std::string text)
{
    inner_.injectKeyEvent(state, code, modifiers, std::move(text));
}

void RemoteWindowImpl::injectText(std::string text)
{
    inner_.injectTextEvent(std::move(text));
}

widget::Introspection RemoteWindowImpl::introspect() const
{
    return introspect_ ? introspect_() : widget::Introspection{};
}

void RemoteWindowImpl::advance(std::chrono::microseconds dt)
{
    remoteTime_ += dt;
    if (frameCallback_)
        frameCallback_(ase::Frame{ remoteTime_, dt });
}

RemotePlatformImpl::RemotePlatformImpl(ase::Platform inner) :
    inner_(std::move(inner))
{
}

ase::PlatformImpl* RemotePlatformImpl::getImplOfType(std::type_index type)
{
    if (type == std::type_index(typeid(RemotePlatformImpl)))
        return this;

    return inner_.getImplOfType(type);
}

ase::Window RemotePlatformImpl::makeWindow(ase::Vector2i size)
{
    auto window = std::make_shared<RemoteWindowImpl>(inner_.makeWindow(size),
            this);
    registry_.push_back(window.get());
    return ase::Window(std::move(window));
}

ase::Window RemotePlatformImpl::makeOffscreenWindow(
        ase::RenderContext& context, ase::Vector2i size)
{
    auto window = std::make_shared<RemoteWindowImpl>(
            inner_.makeOffscreenWindow(context, size), this);
    registry_.push_back(window.get());
    return ase::Window(std::move(window));
}

void RemotePlatformImpl::handleEvents()
{
    inner_.handleEvents();
}

ase::RenderContext RemotePlatformImpl::makeRenderContext()
{
    return inner_.makeRenderContext();
}

ase::Session RemotePlatformImpl::makeSession(ase::RenderContext& context)
{
    // A decorator forward: the remote path drives frames through runSession, not
    // a Session, so the App never asks the remote platform for one -- but the
    // interface still requires it, and forwarding keeps the wrapped backend's
    // own Session reachable, like every other unspecialised call here.
    return inner_.makeSession(context);
}

void RemotePlatformImpl::requestFrame()
{
    inner_.requestFrame();
}

RemoteWindows RemotePlatformImpl::liveWindows() const
{
    RemoteWindows windows;
    windows.reserve(registry_.size());
    for (RemoteWindowImpl* window : registry_)
        windows.push_back(*window);
    return windows;
}

void RemotePlatformImpl::unregisterWindow(RemoteWindowImpl* window)
{
    registry_.erase(
            std::remove(registry_.begin(), registry_.end(), window),
            registry_.end());
}

ase::Platform makeRemotePlatform(ase::Platform inner)
{
    return ase::Platform(std::make_shared<RemotePlatformImpl>(
                std::move(inner)));
}

} // namespace bqui::remote
