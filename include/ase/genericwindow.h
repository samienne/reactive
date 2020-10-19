#pragma once

#include "pointerdragevent.h"
#include "pointerbuttonevent.h"
#include "pointermoveevent.h"
#include "keyevent.h"
#include "hoverevent.h"

#include <btl/option.h>

#include <functional>
#include <string>

namespace ase
{
    class GenericWindow
    {
    public:
        GenericWindow(Vector2i size, float scalingFactor);

        Vector2i getSize() const;
        Vector2i getScaledSize() const;
        void setScalingFactor(float factor);
        float getScalingFactor() const;
        float getWidth() const;
        float getHeight() const;

        std::string const& getTitle() const;
        void setTitle(std::string title);

        void notifyClose();
        void resize(Vector2i size);
        void notifyRedraw();

        void injectPointerButtonEvent(
                unsigned int pointerIndex,
                unsigned int buttonIndex,
                Vector2f pos,
                ButtonState buttonState);

        void injectPointerMoveEvent(unsigned int pointerIndex, Vector2f pos);
        void injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
                bool state);
        void injectKeyEvent(KeyState keyState, KeyCode keyCode,
                uint32_t modifiers, std::string text);

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

    private:
        std::function<void()> closeCallback_;
        std::function<void()> resizeCallback_;
        std::function<void()> redrawCallback_;
        std::function<void(PointerButtonEvent const& e)> buttonCallback_;
        std::function<void(PointerMoveEvent const& e)> pointerCallback_;
        std::function<void(PointerDragEvent const& e)> dragCallback_;
        std::function<void(KeyEvent const& e)> keyCallback_;
        std::function<void(HoverEvent const& e)> hoverCallback_;

        Vector2i size_;
        float scalingFactor_;

        btl::option<Vector2f> pointerPos_;
        bool hover_ = false;

        std::array<bool, 15> buttonPressedState_ = { {
            false, false, false, false,
            false, false, false, false,
            false, false, false, false,
            false, false, false
        }};

        std::array<Vector2f, 15> buttonDownPos_;

        std::string title_;
    };
} // namespace ase

