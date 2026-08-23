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

    struct WindowPresentSync;

    /**
     * @brief The shared base every backend window derives from.
     *
     * Provides the render-context binding, the per-window present backpressure,
     * and the `GenericWindow` backends forward their callbacks and event
     * injection to; a backend implements only its surface-specific behaviour.
     */
    class ASE_EXPORT WindowBase : public WindowImpl
    {
    public:
        RenderContext& getRenderContext() override;

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
         * through, and size its `GenericWindow`.
         *
         * Every backend window passes the context it was made against together
         * with its initial size and scaling factor.
         */
        WindowBase(RenderContext& context, Vector2i size, float scalingFactor);

        GenericWindow genericWindow_;

    private:
        friend class PlatformBase;

        virtual bool needsRedraw() const = 0;

        virtual std::optional<std::chrono::microseconds> frame(
                Frame const& frame) = 0;

        /** @brief Whether this window has a spare in-flight slot, without
         * blocking.
         *
         * Bounds how many of its frames are queued on the GPU at once; false
         * when the budget is full.
         */
        bool canAcquire() const;

        /** @brief Take an in-flight slot and submit this frame's fence on the
         * window's own queue behind its draws.
         *
         * The fence completion frees the slot, preserving the queue's
         * draws -> present -> fence order, and runs `onSlotFreed` -- off the
         * loop thread on a GL backend.
         */
        void submitFrameFence(std::function<void()> onSlotFreed);

        RenderContext context_;

        std::shared_ptr<WindowPresentSync> presentSync_;
    };
}
