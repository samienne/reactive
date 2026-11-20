#pragma once

#include "framebufferimpl.h"
#include "renderbuffer.h"
#include "texture.h"

#include "asevisibility.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>

namespace ase
{
    /**
     * @brief A framebuffer that only records the targets attached to it.
     *
     * A backend builds its own objects from snapshot().
     */
    class ASE_EXPORT AttachmentFramebuffer final :
        public FramebufferImpl,
        public std::enable_shared_from_this<AttachmentFramebuffer>
    {
    public:
        static constexpr size_t maxColorTargets = 8;

        /**
         * @brief The targets attached at one time.
         *
         * A colour index holds a texture or a renderbuffer, never both.
         */
        struct Attachments
        {
            std::array<std::optional<Texture>, maxColorTargets> color;
            std::array<std::optional<Renderbuffer>, maxColorTargets>
                colorBuffers;
            std::optional<Renderbuffer> depth;
        };

        struct Snapshot
        {
            uint64_t version;
            Attachments attachments;
        };

        AttachmentFramebuffer();

        /**
         * @brief An identifier no other framebuffer in the process has had.
         */
        uint64_t getId() const noexcept;

        /**
         * @brief A number that changes whenever a target is set or unset.
         */
        uint64_t getVersion() const noexcept;

        /**
         * @brief The current attachments and the version they belong to.
         */
        Snapshot snapshot() const;

        // From FramebufferImpl
        AttachmentFramebuffer const* asAttachments() const noexcept override;

        void setColorTarget(size_t index, Texture texture) override;
        void setColorTarget(size_t index, Renderbuffer buffer) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;

        void setStencilTarget(Renderbuffer buffer) override;
        void unsetStencilTarget() override;

        Format getColorFormat() const override;
        std::optional<Format> getDepthFormat() const override;

    private:
        uint64_t const id_;
        std::atomic<uint64_t> version_;
        mutable std::mutex mutex_;
        Attachments attachments_;
    };
} // namespace ase
