#pragma once

#include "framebuffer.h"
#include "windowbase.h"

namespace ase
{
    class RenderContext;

    class ASE_EXPORT DummyWindow : public WindowBase
    {
    public:
        DummyWindow(RenderContext& context, Vector2i size);

        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Framebuffer& getDefaultFramebuffer() override;

        void requestFrame() override;

        /** @brief No-op: the dummy backend never draws or swaps. */
        PresentStatus present() override;

    private:
        bool needsRedraw() const override;

        // Drive the stored frame callback for one frame, so the platform's
        // frame loop can advance this window.
        std::optional<std::chrono::microseconds> frame(
                Frame const& frame) override;

        Framebuffer defaultFramebuffer_;
        bool visible_ = false;
    };
} // namespace ase
