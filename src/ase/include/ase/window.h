#pragma once

#include "textevent.h"
#include "keyevent.h"
#include "hoverevent.h"
#include "pointerdragevent.h"
#include "pointermoveevent.h"
#include "pointerbuttonevent.h"
#include "asevisibility.h"
#include "framebuffer.h"
#include "vector.h"

#include <btl/visibility.h>

#include <cstdint>
#include <optional>
#include <chrono>
#include <string>
#include <memory>
#include <functional>

namespace ase
{
    class Framebuffer;
    class WindowImpl;

    struct Frame
    {
        std::chrono::microseconds time;
        std::chrono::microseconds dt;
    };

    class ASE_EXPORT Window
    {
    public:
        Window(std::shared_ptr<WindowImpl>&& impl);
        ~Window();

        Window(Window const&) = default;
        Window(Window&&) noexcept = default;

        Window& operator=(Window const&) = default;
        Window& operator=(Window&&) = default;

        void setVisible(bool value);
        bool isVisible() const;

        void setTitle(std::string title);
        std::string const& getTitle() const;

        Vector2i getSize() const;
        Vector2i getScaledSize() const;
        float getScalingFactor() const;
        Framebuffer& getDefaultFramebuffer();

        void requestFrame();

        void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(
                    Frame const&)> func);
        void setCloseCallback(std::function<void()> func);
        void setResizeCallback(std::function<void()> func);
        void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb);
        void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb);
        void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb);
        void setKeyCallback(std::function<void(KeyEvent const&)> cb);
        void setHoverCallback(std::function<void(HoverEvent const&)> cb);
        void setTextCallback(std::function<void(TextEvent const&)> cb);

        /** @brief Drive the window with a pointer button event as if it came
         * from the platform. Works uniformly across every backend. */
        void injectPointerButtonEvent(unsigned int pointerIndex,
                unsigned int buttonIndex, Vector2f pos, ButtonState buttonState);

        /** @brief Drive the window with a pointer move event. */
        void injectPointerMoveEvent(unsigned int pointerIndex, Vector2f pos);

        /** @brief Drive the window with a hover enter/leave event. */
        void injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
                bool state);

        /** @brief Drive the window with a key event. */
        void injectKeyEvent(KeyState keyState, KeyCode keyCode,
                uint32_t modifiers, std::string text);

        /** @brief Drive the window with a text-input event. */
        void injectTextEvent(std::string text);

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

        template <class T>
        T& getImpl()
        {
            return reinterpret_cast<T&>(*d());
        }

    protected:
        std::shared_ptr<WindowImpl> deferred_;
        inline WindowImpl* d() { return deferred_.get(); }
        inline WindowImpl const* d() const { return deferred_.get(); }
    };
}

