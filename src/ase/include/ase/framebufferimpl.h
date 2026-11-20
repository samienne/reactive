#pragma once

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class Texture;
    class Renderbuffer;
    class AttachmentFramebuffer;

    class ASE_EXPORT FramebufferImpl
    {
    public:
        virtual ~FramebufferImpl() = default;

        /**
         * @brief This framebuffer as a description of its attachments.
         *
         * Null unless the framebuffer is described by attachments; window
         * framebuffers and the GL backends' framebuffers are not.
         */
        virtual AttachmentFramebuffer const* asAttachments() const noexcept;

        virtual void setColorTarget(size_t index, Texture texture) = 0;
        virtual void unsetColorTarget(size_t index) = 0;

        virtual void setDepthTarget(Renderbuffer buffer) = 0;
        virtual void unsetDepthTarget() = 0;
    };
} // namespace ase

