#pragma once

#include <ase/attachmentframebuffer.h>

#include <cstdint>
#include <memory>
#include <unordered_map>

namespace ase
{
    /**
     * @brief Backend framebuffer objects built from attachment
     * descriptions, one per framebuffer.
     *
     * `create` takes an `AttachmentFramebuffer::Snapshot const&` and returns
     * a handle; `destroy` takes a handle. A cache is used from one thread.
     */
    template <typename Handle>
    class FramebufferObjectCache
    {
    public:
        /**
         * @brief The handle for the framebuffer's current attachments.
         *
         * Builds one when there is none, and destroys and rebuilds it when
         * the attachments have changed since it was built.
         */
        template <typename Create, typename Destroy>
        Handle get(AttachmentFramebuffer const& framebuffer,
            Create&& create,
            Destroy&& destroy)
        {
            uint64_t const id = framebuffer.getId();
            auto i = entries_.find(id);
            if (i != entries_.end())
            {
                if (i->second.version == framebuffer.getVersion())
                    return i->second.handle;

                destroy(i->second.handle);
                entries_.erase(i);
            }

            AttachmentFramebuffer::Snapshot const snapshot =
                framebuffer.snapshot();
            Handle handle = create(snapshot);
            entries_.emplace(id,
                Entry{ framebuffer.weak_from_this(),
                    snapshot.version,
                    handle });
            return handle;
        }

        /**
         * @brief Destroys the handles of framebuffers that no longer exist.
         */
        template <typename Destroy>
        void sweep(Destroy&& destroy)
        {
            for (auto i = entries_.begin(); i != entries_.end();)
            {
                if (i->second.owner.expired())
                {
                    destroy(i->second.handle);
                    i = entries_.erase(i);
                }
                else
                    ++i;
            }
        }

        /**
         * @brief Destroys every handle.
         */
        template <typename Destroy>
        void clear(Destroy&& destroy)
        {
            for (auto& entry : entries_)
                destroy(entry.second.handle);
            entries_.clear();
        }

        size_t size() const noexcept
        {
            return entries_.size();
        }

        bool empty() const noexcept
        {
            return entries_.empty();
        }

    private:
        struct Entry
        {
            std::weak_ptr<AttachmentFramebuffer const> owner;
            uint64_t version;
            Handle handle;
        };

        std::unordered_map<uint64_t, Entry> entries_;
    };
} // namespace ase
