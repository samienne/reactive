#pragma once

#include "effectplan.h"
#include "vector.h"
#include "avgvisibility.h"

#include <ase/framebuffer.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace avg
{
    /**
     * @brief A scratch texture and the framebuffer that draws into it.
     *
     * The framebuffer has no depth attachment; size is the texture's, the
     * screen size divided by the texture's divisor and rounded up.
     */
    struct AVG_EXPORT ScratchTarget
    {
        ase::Texture texture;
        ase::Framebuffer framebuffer;
        Vector2i size;
    };

    /**
     * @brief How many frames in a row a scratch texture goes unacquired
     * before ScratchPool::endFrame releases it at the end of the last.
     */
    constexpr size_t scratchIdleFrameLimit = 120;

    /**
     * @brief The scratch textures of one window: a pool per ScratchKey,
     * every texture full-screen at its key's divisor.
     *
     *     pool.beginFrame(screenSize);
     *     ScratchTarget target = pool.acquire(slot);
     *     ...
     *     pool.endFrame();
     *
     * acquire makes the slot's texture on first use, so a frame's slots,
     * taken from a ScratchPlan, allocate what the plan's pool sizes say and
     * no more. A texture persists across frames; beginFrame with a new
     * screen size releases every texture, and endFrame releases one that
     * no frame has acquired for scratchIdleFrameLimit frames. Acquiring
     * before the first beginFrame throws std::logic_error.
     */
    class AVG_EXPORT ScratchPool
    {
    public:
        explicit ScratchPool(ase::RenderContext& context);
        ScratchPool(ScratchPool const&) = delete;
        ScratchPool(ScratchPool&&) noexcept;
        ~ScratchPool();

        ScratchPool& operator=(ScratchPool const&) = delete;
        ScratchPool& operator=(ScratchPool&&) = delete;

        void beginFrame(Vector2i screenSize);

        ScratchTarget acquire(ScratchSlot const& slot);

        void endFrame();

        Vector2i getScreenSize() const;

        /**
         * @brief How many textures the pool holds right now.
         */
        size_t getTextureCount() const;

    private:
        struct Entry
        {
            ScratchTarget target;
            size_t idleFrames = 0;
        };

        struct Pool
        {
            ScratchKey key;
            std::vector<std::optional<Entry>> entries;
        };

        Pool& poolFor(ScratchKey const& key);

        ase::RenderContext& context_;
        Vector2i screenSize_ = Vector2i(0, 0);
        bool sized_ = false;
        std::vector<Pool> pools_;
    };
}
