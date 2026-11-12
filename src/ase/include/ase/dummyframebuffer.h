#pragma once

#include "framebufferimpl.h"
#include "format.h"

#include <optional>

namespace ase
{
    class ASE_EXPORT DummyFramebuffer : public FramebufferImpl
    {
    public:
        /**
         * @brief A framebuffer whose formats are those of the targets
         * attached to it.
         */
        DummyFramebuffer() = default;

        /**
         * @brief A window's framebuffer, with formats fixed as a platform's
         * pixel format fixes them.
         */
        DummyFramebuffer(Format colorFormat, std::optional<Format> depthFormat);

        void setColorTarget(size_t index, Texture texture) override;
        void setColorTarget(size_t index, Renderbuffer texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;

        void setStencilTarget(Renderbuffer buffer) override;
        void unsetStencilTarget() override;

        Format getColorFormat() const override;
        std::optional<Format> getDepthFormat() const override;

    private:
        Format colorFormat_ = FORMAT_UNKNOWN;
        std::optional<Format> depthFormat_;
    };
} // namespace ase
