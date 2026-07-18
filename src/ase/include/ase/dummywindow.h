#pragma once

#include "framebuffer.h"
#include "genericwindow.h"
#include "windowimpl.h"

namespace ase
{
    class ASE_EXPORT DummyWindow : public WindowImpl
    {
    public:
        DummyWindow(Vector2i size);

        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Vector2i getSize() const override;
        float getScalingFactor() const override;
        Framebuffer& getDefaultFramebuffer() override;

        void requestFrame() override;

        void setFrameCallback(
                std::function<std::optional<std::chrono::microseconds>(
                    Frame const&)>) override;
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

        void injectPointerButtonEvent(unsigned int pointerIndex,
                unsigned int buttonIndex, Vector2f pos,
                ButtonState buttonState) override;
        void injectPointerMoveEvent(unsigned int pointerIndex,
                Vector2f pos) override;
        void injectHoverEvent(unsigned int pointerIndex, Vector2f pos,
                bool state) override;
        void injectKeyEvent(KeyState keyState, KeyCode keyCode,
                uint32_t modifiers, std::string text) override;
        void injectTextEvent(std::string text) override;

        // Drive the stored frame callback for one frame. The headless platform
        // calls this to advance a window.
        bool needsRedraw() const;
        std::optional<std::chrono::microseconds> frame(Frame const& frame);

    private:
        GenericWindow genericWindow_;
        Framebuffer defaultFramebuffer_;
        bool visible_ = false;
    };
} // namespace ase
