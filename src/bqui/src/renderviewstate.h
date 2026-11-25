#pragma once

#include "bqui/renderview.h"

#include <ase/framebuffer.h>
#include <ase/renderbuffer.h>
#include <ase/texture.h>
#include <ase/vector.h>
#include <ase/window.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>
#include <vector>

namespace ase
{
    class RenderContext;
}

namespace bqui::detail
{
    struct RenderHandleCore
    {
        void wake();

        void addWaker(std::weak_ptr<void const> owner,
                std::function<void()> waker);

        // Held while a callback runs, so detaching waits for it. Recursive:
        // a callback may destroy its own handle.
        std::recursive_mutex callbackMutex;

        std::mutex mutex;
        std::shared_ptr<RenderHandle::Callback> callback;
        std::shared_ptr<RenderHandle::Release> release;
        bool continuous = false;
        uint64_t frameRequests = 0;
        std::vector<std::pair<std::weak_ptr<void const>,
            std::function<void()>>> wakers;
    };

    struct RenderViewState
    {
        struct Targets
        {
            ase::Texture texture;
            ase::Framebuffer framebuffer;
            ase::Vector2i size;
        };

        explicit RenderViewState(std::shared_ptr<RenderHandleCore> handle);

        /**
         * Recreates the targets when @p size differs, cleared to transparent.
         */
        ase::Texture prepare(ase::RenderContext& context, ase::Vector2i size);

        std::optional<ase::Texture> getTexture() const;

        std::shared_ptr<RenderHandleCore> const handle;

        mutable std::mutex mutex;
        std::optional<Targets> targets;
        bool resized = false;
        uint64_t seenFrameRequests = 0;
    };

    struct RenderViewAccess
    {
        static RenderView make(std::shared_ptr<RenderViewState> state);

        static std::shared_ptr<RenderViewState> const& getState(
                RenderView const& view);
    };

    /**
     * Collects the views drawn while it is current on this thread, and
     * remembers them for releaseRenderViews().
     */
    class RenderViewCollector
    {
    public:
        explicit RenderViewCollector(ase::RenderContext& context);
        ~RenderViewCollector();

        RenderViewCollector(RenderViewCollector const&) = delete;
        RenderViewCollector& operator=(RenderViewCollector const&) = delete;

        static RenderViewCollector* current();

        ase::RenderContext& getContext() const;

        void add(std::shared_ptr<RenderViewState> view);

        std::vector<std::shared_ptr<RenderViewState>> take() &&;

    private:
        ase::RenderContext& context_;
        RenderViewCollector* previous_;
        std::vector<std::shared_ptr<RenderViewState>> views_;
    };

    /**
     * Runs the callbacks of the handles owning @p views for one window
     * frame. Returns whether any wants another frame.
     *
     * @p wakeOwner keeps @p wake registered with those handles while alive.
     */
    bool renderViews(ase::RenderContext& context, ase::Frame const& frame,
            std::vector<std::shared_ptr<RenderViewState>> const& views,
            std::weak_ptr<void const> wakeOwner,
            std::function<void()> const& wake);

    /**
     * Drops the targets of every view collected on this thread and runs
     * their handles' release callbacks. Call before the context goes away.
     */
    void releaseRenderViews();
} // namespace bqui::detail
