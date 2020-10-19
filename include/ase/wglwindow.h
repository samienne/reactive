#pragma once

#include "genericwindow.h"
#include "framebuffer.h"
#include "windowimpl.h"

#include <windows.h>

namespace ase
{
    class WglPlatform;

    class ASE_EXPORT WglWindow : public WindowImpl
    {
    public:
        WglWindow(WglPlatform& platform, Vector2i size, float scalingFactor);

        HWND getHwnd() const;
        HDC getDc() const;

        void present();

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

        LRESULT handleWindowsEvent(HWND hwnd, UINT uMsg, WPARAM wParam,
                LPARAM lParam);

    private:
        void capturePointer();
        void releasePointer();

    private:
        WglPlatform& platform_;
        HWND hwnd_;
        GenericWindow genericWindow_;
        Framebuffer defaultFramebuffer_;
        bool visible_ = false;
        std::string title_;
        int captureCount_ = 0;
    };
} // namespace ase

