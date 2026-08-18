#pragma once

#include "genericwindow.h"
#include "windowimpl.h"
#include "framebuffer.h"
#include "texture.h"
#include "renderbuffer.h"
#include "format.h"
#include "vector.h"

#include "asevisibility.h"

namespace ase
{
    class RenderContext;

    /**
     * @brief A window that renders offscreen with the real backend and shows
     * nothing, so a platform can draw a full frame with no visible window.
     *
     * `setVisible` is a no-op and there is no present. It is a plain
     * `WindowImpl`, driven by the platform run loop like any other window once
     * the platform registers it.
     */
    class ASE_EXPORT OffscreenWindow : public WindowImpl
    {
    public:
        OffscreenWindow(RenderContext& context, Vector2i size);

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

        /** @brief No-op: an offscreen surface has no drawable to swap. */
        PresentStatus present() override;

    private:
        bool needsRedraw() const override;

        // Drive the stored frame callback for one frame, so the platform's
        // frame loop can advance this window.
        std::optional<std::chrono::microseconds> frame(
                Frame const& frame) override;

        GenericWindow genericWindow_;
        Texture texture_;
        Renderbuffer depthbuffer_;
        Framebuffer framebuffer_;
    };
} // namespace ase
