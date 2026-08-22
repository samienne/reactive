#pragma once

#include "framebuffer.h"
#include "windowbase.h"

#include <windows.h>

namespace ase
{
    class WglPlatform;
    class RenderContext;

    class ASE_EXPORT WglWindow : public WindowBase
    {
    public:
        WglWindow(WglPlatform& platform, RenderContext& context, Vector2i size,
                float scalingFactor);

        WglWindow(WglWindow const&) = delete;
        WglWindow& operator=(WglWindow const&) = delete;

        ~WglWindow();

        HWND getHwnd() const;
        HDC getDc() const;

        PresentStatus present() override;

        // From WindowImpl
        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Framebuffer& getDefaultFramebuffer() override;

        void requestFrame() override;

        LRESULT handleWindowsEvent(HWND hwnd, UINT uMsg, WPARAM wParam,
                LPARAM lParam);

    private:
        bool needsRedraw() const override;
        std::optional<std::chrono::microseconds> frame(
                Frame const& frame) override;

        void capturePointer();
        void releasePointer();

    private:
        WglPlatform& platform_;
        HWND hwnd_;
        HDC hdc_;
        Framebuffer defaultFramebuffer_;
        bool visible_ = false;
        std::string title_;
        int captureCount_ = 0;
    };
} // namespace ase

