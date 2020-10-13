#pragma once

#include "framebuffer.h"
#include "windowimpl.h"

#include <windows.h>

namespace ase
{
    class ASE_EXPORT WglWindow : public WindowImpl
    {
    public:
        WglWindow(Vector2i size);

        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Vector2i getSize() const override;
        float getScalingFactor() const override;
        Framebuffer& getDefaultFramebuffer() override;

        void clear() override;

        void setCloseCallback(std::function<void()> func) override;
        void setResizeCallback(std::function<void()> func) override;
        void setRedrawCallback(std::function<void()> func) override;
        void setButtonCallback(
                std::function<void(PointerButtonEvent const&)> cb) override;
        void setPointerCallback(
                std::function<void(PointerMoveEvent const&)> cb) override;
        void setDragCallback(
                std::function<void(PointerDragEvent const&)> cb) override;
        void setKeyCallback(std::function<void(KeyEvent const&)> cb) override;
        void setHoverCallback(std::function<void(HoverEvent const&)> cb) override;

    private:
        HWND hwnd_;
        Framebuffer defaultFramebuffer_;
        bool visible_ = false;
        std::string title_;
    };
} // namespace ase

