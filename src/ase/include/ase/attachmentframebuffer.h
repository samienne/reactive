#pragma once

#include "framebufferimpl.h"
#include "renderbuffer.h"
#include "texture.h"
#include "vector.h"

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
     * A backend reads the attachments when it executes a command that
     * targets the framebuffer, and builds its own objects from them, one
     * set per queue, rebuilt when the version changes.
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
         * `size` is the size of colour target 0, (0, 0) when there is none.
         */
        struct Attachments
        {
            std::array<std::optional<Texture>, maxColorTargets> color;
            std::optional<Renderbuffer> depth;
            Vector2i size = Vector2i(0, 0);
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
         * @brief The counter behind getVersion(), which outlives the
         * framebuffer.
         *
         * Lets a holder see a change without owning the framebuffer.
         */
        std::shared_ptr<std::atomic<uint64_t> const> getVersionCounter()
            const;

        /**
         * @brief The current attachments and the version they belong to.
         */
        Snapshot snapshot() const;

        // FramebufferImpl
        AttachmentFramebuffer const* asAttachments() const noexcept override;

        void setColorTarget(size_t index, Texture texture) override;
        void unsetColorTarget(size_t index) override;

        void setDepthTarget(Renderbuffer buffer) override;
        void unsetDepthTarget() override;


        Format getColorFormat() const override;
        std::optional<Format> getDepthFormat() const override;

    private:
        void updateSize();

    private:
        uint64_t const id_;
        std::shared_ptr<std::atomic<uint64_t>> const version_;
        mutable std::mutex mutex_;
        Attachments attachments_;
    };
} // namespace ase
