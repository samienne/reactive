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

#include <optional>
#include <chrono>
#include <string>
#include <memory>
#include <functional>
#include <typeindex>
#include <typeinfo>

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

        /** @brief The window's implementation as concrete type `T`, reached
         * through any decorators wrapping it; throws `std::bad_cast` if no impl
         * in the chain has that type. Same-binary only. */
        template <class T>
        T const& getImpl() const
        {
            return const_cast<Window*>(this)->getImpl<T>();
        }

        template <class T>
        T& getImpl()
        {
            WindowImpl* impl = getImplOfType(std::type_index(typeid(T)));
            if (!impl)
                throw std::bad_cast();

            return static_cast<T&>(*impl);
        }

        /** @brief Reach the impl of a given concrete type through any decorators
         * wrapping this window, or null if none in the chain has that type,
         * where `getImpl` would throw. Same-binary only. */
        WindowImpl* getImplOfType(std::type_index type);

    protected:
        std::shared_ptr<WindowImpl> deferred_;
        inline WindowImpl* d() { return deferred_.get(); }
        inline WindowImpl const* d() const { return deferred_.get(); }
    };
}

