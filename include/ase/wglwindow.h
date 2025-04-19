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

        std::optional<std::chrono::microseconds> frame(Frame const& frame);
        bool needsRedraw() const;

        // From WindowImpl
        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Vector2i getSize() const override;
        float getScalingFactor() const override;
        Framebuffer& getDefaultFramebuffer() override;

        void requestFrame() override;

        void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(Frame const&)>)
            override;
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

        LRESULT handleWindowsEvent(HWND hwnd, UINT uMsg, WPARAM wParam,
                LPARAM lParam);

    private:
        void capturePointer();
        void releasePointer();

    private:
        WglPlatform& platform_;
        HWND hwnd_;
        HDC hdc_;
        GenericWindow genericWindow_;
        Framebuffer defaultFramebuffer_;
        bool visible_ = false;
        std::string title_;
        int captureCount_ = 0;
    };
} // namespace ase

