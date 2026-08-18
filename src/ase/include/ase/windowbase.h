#pragma once

#include "windowimpl.h"
#include "genericwindow.h"
#include "rendercontext.h"
#include "presentstatus.h"
#include "vector.h"

#include <btl/visibility.h>

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>

namespace ase
{
    class RenderQueue;
    struct Frame;

    // The per-window in-flight bookkeeping backing acquire(); defined in
    // windowbase.cpp so the mutex/condition_variable stay out of the header.
    struct WindowPresentSync;

    /**
     * @brief The shared base every backend window derives from: it holds the
     * render-context binding, the per-window present backpressure, and the
     * `GenericWindow` all backends forward their callbacks and event injection
     * to. `WindowImpl` stays a pure interface; this is where the common
     * implementation lives.
     */
    class ASE_EXPORT WindowBase : public WindowImpl
    {
    public:
        /** @brief The render context this window was created against and both
         * renders and presents through. Bound at creation and non-transferable:
         * the window carries its own (surface, queue) binding. */
        RenderContext& getRenderContext();

        /** @brief This window's main render queue -- the one FIFO its draws,
         * its present, and its backpressure fence all share, so their ordering
         * holds. */
        RenderQueue getMainRenderQueue();

        void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(
                    Frame const&)> cb) override;
        void setCloseCallback(std::function<void()> func) override;
        void setResizeCallback(std::function<void()> func) override;
        void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb) override;
        void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb) override;
        void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb) override;
        void setKeyCallback(std::function<void(KeyEvent const&)> cb) override;
        void setHoverCallback(std::function<void(HoverEvent const&)> cb) override;
        void setTextCallback(std::function<void(TextEvent const&)> cb) override;

        void injectPointerButtonEvent(unsigned int pointerIndex,
                unsigned int buttonIndex, Vector2f pos,
                ButtonState buttonState) override;
        void injectPointerMoveEvent(unsigned int pointerIndex,
                Vector2f pos) override;
        void injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
                bool state) override;
        void injectKeyEvent(KeyState keyState, KeyCode keyCode,
                uint32_t modifiers, std::string text) override;
        void injectTextEvent(std::string text) override;

        Vector2i getSize() const override;
        float getScalingFactor() const override;

    protected:
        /** @brief Bind the window to the render context it renders and presents
         * through, and size its `GenericWindow`; every backend window passes the
         * context it was made against together with its initial size and scaling
         * factor. */
        WindowBase(RenderContext& context, Vector2i size, float scalingFactor);

        // The event/callback state every backend shares; backends mutate it
        // during OS-event translation, so it is reachable to derived classes.
        GenericWindow genericWindow_;

    private:
        // The render-polling and frame-pacing surface the platform's frame loop
        // drives. Off the public window interface: only that loop, which owns the
        // render relationship, asks whether a window wants drawing, gates it
        // against the window's own backpressure, draws it, and fences it. Uniform
        // across real and offscreen windows.
        friend class PlatformImpl;

        virtual bool needsRedraw() const = 0;

        virtual std::optional<std::chrono::microseconds> frame(
                Frame const& frame) = 0;

        // Backpressure lives on the window: gate frame production against this
        // window's own in-flight count on its own queue.

        /** @brief Block until this window has a spare in-flight slot, so no more
         * than a fixed number of its frames are queued on the GPU at once. GL
         * always reports `Ok`; the status is the seam a backend whose acquire
         * can fail (a lost swapchain) reports through. */
        PresentStatus acquire();

        /** @brief Take an in-flight slot and submit this frame's fence on the
         * window's own queue behind its draws; the fence completion frees the
         * slot. Keeps the queue's draws -> present -> fence order. */
        void submitFrameFence();

        // The (surface, queue) binding: the context this window renders and
        // presents through, held for its whole life. Co-owned (a strong handle,
        // not a bare reference) so the window keeps its context -- and through
        // it the platform -- alive, and teardown follows destruction order.
        RenderContext context_;

        std::shared_ptr<WindowPresentSync> presentSync_;
    };
}
