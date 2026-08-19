#include "windowbase.h"

#include "rendercontext.h"
#include "renderqueue.h"
#include "commandbuffer.h"

#include <tracy/Tracy.hpp>

#include <condition_variable>
#include <mutex>

namespace ase
{

// The window's backpressure state, shared with the fence completion callback so
// a fence can free a slot even after the window itself is gone.
struct WindowPresentSync
{
    std::mutex mutex;
    std::condition_variable slotFreed;

    // Frames whose draws are submitted but whose fence has not yet completed.
    int inFlight = 0;

    // How many of this window's frames may be in flight at once. Two keeps the
    // GPU fed (one presenting while the next builds) without unbounded latency.
    int budget = 2;
};

WindowBase::WindowBase(RenderContext& context, Vector2i size,
        float scalingFactor) :
    genericWindow_(size, scalingFactor),
    context_(context.getSharedImpl()),
    presentSync_(std::make_shared<WindowPresentSync>())
{
}

RenderContext& WindowBase::getRenderContext()
{
    return context_;
}

PresentStatus WindowBase::acquire()
{
    ZoneScopedN("acquire");

    auto sync = presentSync_;

    std::unique_lock<std::mutex> lock(sync->mutex);
    sync->slotFreed.wait(lock, [&sync] { return sync->inFlight < sync->budget; });

    return PresentStatus::Ok;
}

void WindowBase::submitFrameFence()
{
    ZoneScopedN("submitFrameFence");

    auto sync = presentSync_;

    {
        std::lock_guard<std::mutex> lock(sync->mutex);
        ++sync->inFlight;
    }

    // The fence completion runs on the queue's render thread (or inline where
    // the backend has no GPU), decrements off the loop thread, and wakes an
    // acquire() waiting for a slot. Capturing the shared state, not the window,
    // keeps it safe if the window is torn down with a fence still pending.
    CommandBuffer commandBuffer;
    commandBuffer.pushFence([sync]
        {
            std::lock_guard<std::mutex> lock(sync->mutex);
            --sync->inFlight;
            sync->slotFreed.notify_all();
        });

    context_.getMainRenderQueue().submit(std::move(commandBuffer));
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
