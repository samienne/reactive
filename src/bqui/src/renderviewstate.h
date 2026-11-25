#pragma once

#include "bqui/renderview.h"

#include <ase/framebuffer.h>
#include <ase/renderbuffer.h>
#include <ase/texture.h>
#include <ase/vector.h>
#include <ase/window.h>

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

        std::mutex mutex;
        std::shared_ptr<RenderHandle::Callback> callback;
        bool continuous = false;
        bool frameRequested = false;
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

        /** Recreates the targets when @p size differs, cleared to transparent.
         */
        ase::Texture prepare(ase::RenderContext& context, ase::Vector2i size);

        std::optional<ase::Texture> getTexture() const;

        std::shared_ptr<RenderHandleCore> const handle;

        mutable std::mutex mutex;
        std::optional<Targets> targets;
        bool resized = false;
    };

    /** Collects the views drawn while it is current on this thread. */
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

    /** Runs the callbacks of the handles owning @p views for one window
     * frame. Returns whether any wants another frame.
     *
     * @p wakeOwner keeps @p wake registered with those handles while alive.
     */
    bool renderViews(ase::RenderContext& context, ase::Frame const& frame,
            std::vector<std::shared_ptr<RenderViewState>> const& views,
            std::weak_ptr<void const> wakeOwner,
            std::function<void()> const& wake);
} // namespace bqui::detail
