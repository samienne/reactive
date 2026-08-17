#pragma once

#include "textevent.h"
#include "keyevent.h"
#include "hoverevent.h"
#include "pointerdragevent.h"
#include "pointermoveevent.h"
#include "pointerbuttonevent.h"
#include "asevisibility.h"
#include "framebuffer.h"
#include "presentstatus.h"
#include "dispatcher.h"
#include "vector.h"

#include <btl/visibility.h>

#include <cstdint>
#include <string>
#include <functional>
#include <chrono>
#include <memory>
#include <optional>
#include <typeindex>
#include <typeinfo>

namespace ase
{
    class Framebuffer;
    class RenderContext;
    class RenderQueue;
    struct Frame;

    // The per-window in-flight bookkeeping backing acquire(); defined in
    // windowimpl.cpp so the mutex/condition_variable stay out of the header.
    struct WindowPresentSync;

    class ASE_EXPORT WindowImpl
    {
    public:
        virtual ~WindowImpl() = default;

        /** @brief The render context this window was created against and both
         * renders and presents through. Bound at creation and non-transferable:
         * the window carries its own (surface, queue) binding. */
        RenderContext& getRenderContext();

        /** @brief This window's main render queue -- the one FIFO its draws,
         * its present, and its backpressure fence all share, so their ordering
         * holds. */
        RenderQueue getMainRenderQueue();

        /** @brief Reach the impl of a given concrete type through any decorators
         * wrapping this window, or null if none in the chain has that type,
         * where `getImpl` would throw. Same-binary only. */
        virtual WindowImpl* getImplOfType(std::type_index type)
        {
            return std::type_index(typeid(*this)) == type ? this : nullptr;
        }

        /** @overload Typed form: the impl as `T*`, or null if none in the chain
         * is a `T`. Same-binary only. */
        template <class T>
        T* getImplOfType()
        {
            return static_cast<T*>(getImplOfType(std::type_index(typeid(T))));
        }

        virtual void setVisible(bool value) = 0;
        virtual bool isVisible() const = 0;

        virtual void setTitle(std::string&& title) = 0;
        virtual std::string const& getTitle() const = 0;

        virtual Vector2i getSize() const = 0;
        virtual float getScalingFactor() const = 0;
        virtual Framebuffer& getDefaultFramebuffer() = 0;

        virtual void requestFrame() = 0;

        /** @brief Present this surface's finished frame. Called on the render
         * thread once the frame's draws have been submitted; a surface with
         * nothing to swap (offscreen) returns `Ok`. */
        virtual PresentStatus present(Dispatched) = 0;

        virtual void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(
                    Frame const&)>) = 0;
        virtual void setCloseCallback(std::function<void()> func) = 0;
        virtual void setResizeCallback(std::function<void()> func) = 0;
        virtual void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb) = 0;
        virtual void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb) = 0;
        virtual void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb) = 0;
        virtual void setKeyCallback(std::function<void(KeyEvent const&)> cb) = 0;
        virtual void setHoverCallback(std::function<void(HoverEvent const&)> cb) = 0;
        virtual void setTextCallback(std::function<void(TextEvent const&)> cb) = 0;

        /** @brief Feed a pointer button event to the window's callbacks as if
         * it came from the platform, for driving the window programmatically. */
        virtual void injectPointerButtonEvent(unsigned int pointerIndex,
                unsigned int buttonIndex, Vector2f pos,
                ButtonState buttonState) = 0;

        /** @brief Feed a pointer move event to the window's callbacks. */
        virtual void injectPointerMoveEvent(unsigned int pointerIndex,
                Vector2f pos) = 0;

        /** @brief Feed a hover enter/leave event to the window's callbacks. */
        virtual void injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
                bool state) = 0;

        /** @brief Feed a key event to the window's callbacks. */
        virtual void injectKeyEvent(KeyState keyState, KeyCode keyCode,
                uint32_t modifiers, std::string text) = 0;

        /** @brief Feed a text-input event to the window's callbacks. */
        virtual void injectTextEvent(std::string text) = 0;

    protected:
        /** @brief Bind the window to the render context it renders and presents
         * through; every backend window passes the context it was made against.
         */
        explicit WindowImpl(RenderContext& context);

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
        // presents through, held for its whole life.
        RenderContext& context_;

        std::shared_ptr<WindowPresentSync> presentSync_;
    };
}

