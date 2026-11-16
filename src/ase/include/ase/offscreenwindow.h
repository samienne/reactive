#pragma once

#include "windowbase.h"
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
     * @brief A window that renders offscreen and shows nothing.
     *
     * Renders a full frame with the real backend but is never shown, so a
     * platform can draw with no visible window. `setVisible` is a no-op and
     * presenting swaps nothing; it serves `captureNextFrame` when the render
     * context can read back. Otherwise it is a plain `WindowBase`, driven by
     * the platform run loop like any other window once the platform registers
     * it.
     */
    class ASE_EXPORT OffscreenWindow : public WindowBase
    {
    public:
        OffscreenWindow(RenderContext& context, Vector2i size);

        void setVisible(bool value) override;
        bool isVisible() const override;

        void setTitle(std::string&& title) override;
        std::string const& getTitle() const override;

        Framebuffer& getDefaultFramebuffer() override;

        bool canCapture() const override;

        void requestFrame() override;

        /**
         * @brief Serve the armed captures; there is no drawable to swap.
         */
        PresentStatus present() override;

    private:
        // Drive the stored frame callback for one frame, so the platform's
        // frame loop can advance this window.
        std::optional<std::chrono::microseconds> frame(
                Frame const& frame) override;

        bool canReadback_;
        Texture texture_;
        Renderbuffer depthbuffer_;
        Framebuffer framebuffer_;
    };
} // namespace ase
