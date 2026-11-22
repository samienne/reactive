#pragma once

#include "framebufferimpl.h"
#include "renderbuffer.h"
#include "texture.h"
#include "vector.h"

#include "asevisibility.h"

#include <btl/uniqueid.h>

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
     * A backend reads the attachments when it executes a command that
     * targets the framebuffer.
     */
    class ASE_EXPORT AttachmentFramebuffer final :
        public FramebufferImpl,
        public std::enable_shared_from_this<AttachmentFramebuffer>
    {
    public:
        /**
         * @brief The number of colour target indices.
         */
        static constexpr size_t maxColorTargets = 8;

        /**
         * @brief The targets attached at one time.
         *
         * `size` is the size of colour target 0, (0, 0) when there is none.
         */
        struct Attachments
        {
            std::array<std::optional<Texture>, maxColorTargets> color;
            std::optional<Renderbuffer> depth;
            Vector2i size = Vector2i(0, 0);
        };

        /**
         * @brief The attachments together with the generation they belong to.
         */
        struct Snapshot
        {
            uint64_t generation;
            Attachments attachments;
        };

        AttachmentFramebuffer();

        /**
         * @brief An identifier no other framebuffer in the process has had.
         */
        btl::UniqueId getId() const noexcept;

        /**
         * @brief A number that changes whenever a target is set or unset.
         */
        uint64_t getGeneration() const noexcept;

        /**
         * @brief The current attachments and the generation they belong to.
         */
        Snapshot snapshot() const;

        // FramebufferImpl
        AttachmentFramebuffer const* asAttachments() const noexcept override;

        void setColorTarget(size_t index, Texture texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;

    private:
        void updateSize();

    private:
        btl::UniqueId const id_;
        std::atomic<uint64_t> generation_;
        mutable std::mutex mutex_;
        Attachments attachments_;
    };
} // namespace ase
