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
    class RenderContext;
    class RenderQueue;
    struct Frame;

    class ASE_EXPORT WindowImpl : public std::enable_shared_from_this<WindowImpl>
    {
    public:
        virtual ~WindowImpl() = default;

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

        /** @brief The render context this window was created against and both
         * renders and presents through. Bound at creation and non-transferable:
         * the window carries its own (surface, queue) binding. */
        virtual RenderContext& getRenderContext() = 0;

        virtual void setVisible(bool value) = 0;
        virtual bool isVisible() const = 0;

        virtual void setTitle(std::string&& title) = 0;
        virtual std::string const& getTitle() const = 0;

        virtual Vector2i getSize() const = 0;
        virtual float getScalingFactor() const = 0;
        virtual Framebuffer& getDefaultFramebuffer() = 0;

        virtual void requestFrame() = 0;

        /** @brief Present this surface's finished frame, sequencing the swap
         * behind this window's own submitted draws on its own queue. Returns
         * immediately without blocking; a surface with nothing to swap
         * (offscreen) returns `Ok`. */
        virtual PresentStatus present() = 0;

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
    };
}
