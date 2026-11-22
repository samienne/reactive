#include "windowbase.h"

#include "rendercontext.h"
#include "renderqueue.h"
#include "commandbuffer.h"
#include "fence.h"

#include <tracy/Tracy.hpp>

#include <algorithm>
#include <cstddef>

namespace ase
{

namespace
{
    // Frames a window may have queued on the GPU at once.
    constexpr std::size_t kFrameBudget = 2;
} // namespace

WindowBase::WindowBase(RenderContext& context, Vector2i size,
        float scalingFactor) :
    genericWindow_(size, scalingFactor),
    context_(context.getSharedImpl())
{
}

RenderContext& WindowBase::getRenderContext()
{
    return context_;
}

std::optional<std::chrono::microseconds>
WindowBase::nextFrameTime() const
{
    return genericWindow_.nextFrameTime();
}

std::chrono::microseconds WindowBase::lastFrameTime() const
{
    return genericWindow_.lastFrameTime();
}

bool WindowBase::canAcquire()
{
    inFlight_.erase(std::remove_if(inFlight_.begin(), inFlight_.end(),
                [](Fence const& fence) { return fence.isSignaled(); }),
            inFlight_.end());

    return inFlight_.size() < kFrameBudget;
}

void WindowBase::submitFrameFence()
{
    ZoneScopedN("submitFrameFence");

    CommandBuffer commandBuffer;
    Fence fence = commandBuffer.pushFence();
    context_.getMainRenderQueue().submit(std::move(commandBuffer));

    inFlight_.push_back(std::move(fence));
}

void WindowBase::setCursor(Cursor cursor)
{
    genericWindow_.setCursor(cursor);
}

Cursor WindowBase::getCursor() const
{
    return genericWindow_.getCursor();
}

Vector2i WindowBase::getSize() const
{
    return genericWindow_.getSize();
}

float WindowBase::getScalingFactor() const
{
    return genericWindow_.getScalingFactor();
}

void WindowBase::setFrameCallback(
        std::function<std::optional<std::chrono::microseconds>(
            Frame const&)> cb)
{
    genericWindow_.setFrameCallback(std::move(cb));
}

void WindowBase::setCloseCallback(std::function<void()> func)
{
    genericWindow_.setCloseCallback(std::move(func));
}

void WindowBase::setResizeCallback(std::function<void()> func)
{
    genericWindow_.setResizeCallback(std::move(func));
}

void WindowBase::setButtonCallback(
        std::function<void(PointerButtonEvent const&)> cb)
{
    genericWindow_.setButtonCallback(std::move(cb));
}

void WindowBase::setPointerCallback(
        std::function<void(PointerMoveEvent const&)> cb)
{
    genericWindow_.setPointerCallback(std::move(cb));
}

void WindowBase::setDragCallback(
        std::function<void(PointerDragEvent const&)> cb)
{
    genericWindow_.setDragCallback(std::move(cb));
}

void WindowBase::setKeyCallback(std::function<void(KeyEvent const&)> cb)
{
    genericWindow_.setKeyCallback(std::move(cb));
}

void WindowBase::setHoverCallback(std::function<void(HoverEvent const&)> cb)
{
    genericWindow_.setHoverCallback(std::move(cb));
}

void WindowBase::setTextCallback(std::function<void(TextEvent const&)> cb)
{
    genericWindow_.setTextCallback(std::move(cb));
}

void WindowBase::injectPointerButtonEvent(unsigned int pointerIndex,
        unsigned int buttonIndex, Vector2f pos, ButtonState buttonState)
{
    genericWindow_.injectPointerButtonEvent(pointerIndex, buttonIndex, pos,
            buttonState);
}

void WindowBase::injectPointerMoveEvent(unsigned int pointerIndex,
        Vector2f pos)
{
    genericWindow_.injectPointerMoveEvent(pointerIndex, pos);
}

void WindowBase::injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
        bool state)
{
    genericWindow_.injectHoverEvent(pointerIndex, pos, state);
}

void WindowBase::injectKeyEvent(KeyState keyState, KeyCode keyCode,
        uint32_t modifiers, std::string text)
{
    genericWindow_.injectKeyEvent(keyState, keyCode, modifiers,
            std::move(text));
}

void WindowBase::injectTextEvent(std::string text)
{
    genericWindow_.injectTextEvent(std::move(text));
}

} // namespace ase
