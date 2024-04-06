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

#include <string>
#include <functional>

namespace ase
{
    class Framebuffer;
    struct FrameInfo;

    class ASE_EXPORT WindowImpl
    {
    public:
        virtual ~WindowImpl() = default;

        virtual void setVisible(bool value) = 0;
        virtual bool isVisible() const = 0;

        virtual void setTitle(std::string&& title) = 0;
        virtual std::string const& getTitle() const = 0;

        virtual Vector2i getSize() const = 0;
        virtual float getScalingFactor() const = 0;
        virtual Framebuffer& getDefaultFramebuffer() = 0;

        virtual void setOnFrameCallback(std::function<void(FrameInfo const&)> func) = 0;
        virtual void setCloseCallback(std::function<void()> func) = 0;
        virtual void setResizeCallback(std::function<void()> func) = 0;
        virtual void setRedrawCallback(std::function<void()> func) = 0;
        virtual void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb) = 0;
        virtual void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb) = 0;
        virtual void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb) = 0;
        virtual void setKeyCallback(std::function<void(KeyEvent const&)> cb) = 0;
        virtual void setHoverCallback(std::function<void(HoverEvent const&)> cb) = 0;
        virtual void setTextCallback(std::function<void(TextEvent const&)> cb) = 0;
    };
}

