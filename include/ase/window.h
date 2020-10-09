#pragma once

#include "keyevent.h"
#include "hoverevent.h"
#include "pointerdragevent.h"
#include "pointermoveevent.h"
#include "pointerbuttonevent.h"
#include "asevisibility.h"

#include <string>
#include <memory>
#include <functional>

namespace ase
{
    class Framebuffer;
    class WindowImpl;

    class ASE_EXPORT Window
    {
    public:
        Window(std::shared_ptr<WindowImpl>&& impl);
        ~Window();

        Window(Window const&) = default;
        Window(Window&&) noexcept = default;

        void setVisible(bool value);
        bool isVisible() const;

        void setTitle(std::string title);
        std::string const& getTitle() const;

        Vector2i getSize() const;
        Vector2i getScaledSize() const;
        float getScalingFactor() const;
        Framebuffer& getDefaultFramebuffer();

        void clear();

        void setCloseCallback(std::function<void()> func);
        void setResizeCallback(std::function<void()> func);
        void setRedrawCallback(std::function<void()> func);
        void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb);
        void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb);
        void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb);
        void setKeyCallback(std::function<void(KeyEvent const&)> cb);
        void setHoverCallback(std::function<void(HoverEvent const&)> cb);

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

