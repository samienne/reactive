#include "scratchpool.h"

#include <stdexcept>

namespace avg
{

ScratchPool::ScratchPool(ase::RenderContext& context) :
    context_(context)
{
}

ScratchPool::ScratchPool(ScratchPool&&) noexcept = default;

ScratchPool::~ScratchPool()
{
}

void ScratchPool::beginFrame(Vector2i screenSize)
{
    if (!sized_ || screenSize != screenSize_)
    {
        pools_.clear();
        screenSize_ = screenSize;
        sized_ = true;
    }
}

ScratchTarget ScratchPool::acquire(ScratchSlot const& slot)
{
    if (!sized_)
        throw std::logic_error("ScratchPool::acquire before beginFrame");

    Pool& pool = poolFor(slot.key);
    if (pool.entries.size() <= slot.index)
        pool.entries.resize(slot.index + 1);

    std::optional<Entry>& entry = pool.entries[slot.index];
    if (!entry)
    {
        Vector2i const size = targetSize(screenSize_, slot.key.divisor);
        ase::Texture texture = context_.makeTexture(size, slot.key.format);
        ase::Framebuffer framebuffer = context_.makeFramebuffer();
        framebuffer.setColorTarget(0, texture);

        entry = Entry{ ScratchTarget{ std::move(texture),
            std::move(framebuffer), size }, 0 };
    }

    entry->idleFrames = 0;

    return entry->target;
}

void ScratchPool::endFrame()
{
    for (Pool& pool : pools_)
    {
        for (std::optional<Entry>& entry : pool.entries)
        {
            if (!entry)
                continue;

            // The frame that acquired the texture counts one as well.
            if (++entry->idleFrames > scratchIdleFrameLimit)
                entry.reset();
        }
    }
}

Vector2i ScratchPool::getScreenSize() const
{
    return screenSize_;
}

size_t ScratchPool::getTextureCount() const
{
    size_t count = 0;
    for (Pool const& pool : pools_)
    {
        for (std::optional<Entry> const& entry : pool.entries)
        {
            if (entry)
                ++count;
        }
    }

    return count;
}

ScratchPool::Pool& ScratchPool::poolFor(ScratchKey const& key)
{
    for (Pool& pool : pools_)
    {
        if (pool.key == key)
            return pool;
    }

    pools_.push_back(Pool{ key, {} });
    return pools_.back();
}

} // namespace avg
