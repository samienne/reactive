#include "effectplan.h"

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace avg
{

namespace
{
    int floorDiv(int a, int b)
    {
        int q = a / b;
        if ((a % b != 0) && ((a < 0) != (b < 0)))
            --q;
        return q;
    }

    int ceilDiv(int a, int b)
    {
        return -floorDiv(-a, b);
    }

    Divisor divisorOf(Pass const& pass)
    {
        return pass.output ? pass.output->divisor : Divisor::One;
    }

    ScratchKey keyOf(PassOutput const& output)
    {
        ScratchKey key;
        key.divisor = output.divisor;
        key.format = output.format;
        return key;
    }
} // anonymous namespace

bool PixelRect::isEmpty() const
{
    return width <= 0 || height <= 0;
}

PixelRect PixelRect::unionWith(PixelRect const& rhs) const
{
    if (isEmpty())
        return rhs.isEmpty() ? PixelRect() : rhs;

    if (rhs.isEmpty())
        return *this;

    PixelRect r;
    r.x = std::min(x, rhs.x);
    r.y = std::min(y, rhs.y);
    r.width = std::max(x + width, rhs.x + rhs.width) - r.x;
    r.height = std::max(y + height, rhs.y + rhs.height) - r.y;
    return r;
}

PixelRect PixelRect::grown(int amount) const
{
    if (isEmpty())
        return PixelRect();

    PixelRect r;
    r.x = x - amount;
    r.y = y - amount;
    r.width = width + 2 * amount;
    r.height = height + 2 * amount;
    return r.isEmpty() ? PixelRect() : r;
}

PixelRect PixelRect::clamped(Vector2i size) const
{
    if (isEmpty())
        return PixelRect();

    int const x0 = std::max(x, 0);
    int const y0 = std::max(y, 0);
    int const x1 = std::min(x + width, static_cast<int>(size[0]));
    int const y1 = std::min(y + height, static_cast<int>(size[1]));

    if (x1 <= x0 || y1 <= y0)
        return PixelRect();

    PixelRect r;
    r.x = x0;
    r.y = y0;
    r.width = x1 - x0;
    r.height = y1 - y0;
    return r;
}

PixelRect PixelRect::rescaled(Divisor from, Divisor to) const
{
    if (isEmpty())
        return PixelRect();

    int const f = toInt(from);
    int const t = toInt(to);

    int const x0 = floorDiv(x * f, t);
    int const y0 = floorDiv(y * f, t);
    int const x1 = ceilDiv((x + width) * f, t);
    int const y1 = ceilDiv((y + height) * f, t);

    PixelRect r;
    r.x = x0;
    r.y = y0;
    r.width = x1 - x0;
    r.height = y1 - y0;
    return r;
}

bool PixelRect::operator==(PixelRect const& rhs) const
{
    return x == rhs.x
        && y == rhs.y
        && width == rhs.width
        && height == rhs.height;
}

bool PixelRect::operator!=(PixelRect const& rhs) const
{
    return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, PixelRect const& r)
{
    return stream << "PixelRect{" << r.x << "," << r.y << " "
        << r.width << "x" << r.height << "}";
}

Vector2i targetSize(Vector2i screenSize, Divisor divisor)
{
    int const d = toInt(divisor);
    return Vector2i(
            ceilDiv(static_cast<int>(screenSize[0]), d),
            ceilDiv(static_cast<int>(screenSize[1]), d));
}

EffectRegions deriveRegions(Effect const& effect, PixelRect shapeBounds,
        Vector2i screenSize)
{
    std::vector<Pass> const& passes = effect.getPasses();

    EffectRegions regions;
    regions.passRegions.resize(passes.size());
    regions.passRegions.back() = shapeBounds;

    for (size_t n = passes.size(); n > 0; --n)
    {
        size_t const i = n - 1;
        Pass const& pass = passes[i];
        Divisor const divisor = divisorOf(pass);

        PixelRect& region = regions.passRegions[i];
        region = region.clamped(targetSize(screenSize, divisor));

        PixelRect const sampled = region.grown(pass.padding);

        for (PassInput const& input : pass.inputs)
        {
            if (input.isBackdrop())
            {
                regions.backdropRegion = regions.backdropRegion.unionWith(
                        sampled.rescaled(divisor, Divisor::One));
            }
            else
            {
                PixelRect& producer = regions.passRegions[input.getPass()];
                producer = producer.unionWith(sampled.rescaled(divisor,
                            divisorOf(passes[input.getPass()])));
            }
        }
    }

    regions.backdropRegion = regions.backdropRegion.clamped(screenSize);

    return regions;
}

bool ScratchKey::operator==(ScratchKey const& rhs) const
{
    return divisor == rhs.divisor && format == rhs.format;
}

bool ScratchKey::operator!=(ScratchKey const& rhs) const
{
    return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, ScratchKey const& key)
{
    return stream << "ScratchKey{" << key.divisor << ", format "
        << static_cast<int>(key.format) << "}";
}

bool ScratchSlot::operator==(ScratchSlot const& rhs) const
{
    return key == rhs.key && index == rhs.index;
}

bool ScratchSlot::operator!=(ScratchSlot const& rhs) const
{
    return !(*this == rhs);
}

std::ostream& operator<<(std::ostream& stream, ScratchSlot const& slot)
{
    return stream << "ScratchSlot{" << slot.key << " #" << slot.index << "}";
}

size_t ScratchPlan::getPoolSize(ScratchKey const& key) const
{
    for (PoolSize const& pool : pools)
    {
        if (pool.key == key)
            return pool.count;
    }

    return 0;
}

ScratchTimeline::ScratchTimeline()
{
}

ScratchTimeline::~ScratchTimeline()
{
}

size_t ScratchTimeline::addSnapshot()
{
    Item item;
    item.snapshot = snapshotCount_++;
    items_.push_back(item);
    return item.snapshot;
}

size_t ScratchTimeline::addEffect(Effect const& effect, size_t snapshot)
{
    if (snapshot >= snapshotCount_)
    {
        throw std::invalid_argument("effect reads snapshot "
                + std::to_string(snapshot) + ", which has not been taken");
    }

    Item item;
    item.effect = &effect;
    item.snapshot = snapshot;
    items_.push_back(item);
    return effectCount_++;
}

size_t ScratchTimeline::getSnapshotCount() const
{
    return snapshotCount_;
}

size_t ScratchTimeline::getEffectCount() const
{
    return effectCount_;
}

std::vector<ScratchTimeline::Item> const& ScratchTimeline::getItems() const
{
    return items_;
}

namespace
{
    ScratchKey snapshotKey()
    {
        return ScratchKey();
    }

    class Pools
    {
    public:
        ScratchSlot allocate(ScratchKey const& key)
        {
            Pool& pool = find(key);

            ScratchSlot slot;
            slot.key = key;

            if (pool.free.empty())
                slot.index = pool.count++;
            else
            {
                slot.index = pool.free.back();
                pool.free.pop_back();
            }

            return slot;
        }

        void release(ScratchSlot const& slot)
        {
            std::vector<size_t>& free = find(slot.key).free;
            auto i = std::upper_bound(free.begin(), free.end(), slot.index,
                    [](size_t a, size_t b)
                    {
                        return a > b;
                    });
            free.insert(i, slot.index);
        }

        std::vector<PoolSize> sizes() const
        {
            std::vector<PoolSize> result;
            for (Pool const& pool : pools_)
            {
                PoolSize size;
                size.key = pool.key;
                size.count = pool.count;
                result.push_back(size);
            }

            return result;
        }

    private:
        struct Pool
        {
            ScratchKey key;
            size_t count = 0;
            // Free slot indices, descending, so the lowest is at the back.
            std::vector<size_t> free;
        };

        Pool& find(ScratchKey const& key)
        {
            for (Pool& pool : pools_)
            {
                if (pool.key == key)
                    return pool;
            }

            Pool pool;
            pool.key = key;
            pools_.push_back(pool);
            return pools_.back();
        }

        std::vector<Pool> pools_;
    };

    struct Value
    {
        ScratchKey key;
        size_t definedAt = 0;
        size_t lastUse = 0;
        ScratchSlot slot;
    };
} // anonymous namespace

ScratchPlan planScratch(ScratchTimeline const& timeline)
{
    std::vector<ScratchTimeline::Item> const& items = timeline.getItems();

    std::vector<Value> values;
    std::vector<size_t> snapshotValues(timeline.getSnapshotCount());
    // Per effect, per pass: the value index of its output, or SIZE_MAX.
    std::vector<std::vector<size_t>> passValues;

    size_t step = 0;
    for (ScratchTimeline::Item const& item : items)
    {
        if (!item.effect)
        {
            Value value;
            value.key = snapshotKey();
            value.definedAt = step;
            value.lastUse = step;
            snapshotValues[item.snapshot] = values.size();
            values.push_back(value);
            ++step;
            continue;
        }

        std::vector<Pass> const& passes = item.effect->getPasses();
        std::vector<size_t> outputs(passes.size(), SIZE_MAX);

        for (size_t i = 0; i < passes.size(); ++i)
        {
            Pass const& pass = passes[i];

            for (PassInput const& input : pass.inputs)
            {
                size_t const v = input.isBackdrop()
                    ? snapshotValues[item.snapshot]
                    : outputs[input.getPass()];
                values[v].lastUse = std::max(values[v].lastUse, step);
            }

            if (pass.output)
            {
                Value value;
                value.key = keyOf(*pass.output);
                value.definedAt = step;
                value.lastUse = step;
                outputs[i] = values.size();
                values.push_back(value);
            }

            ++step;
        }

        passValues.push_back(std::move(outputs));
    }

    std::vector<std::vector<size_t>> releaseAt(step);
    for (size_t v = 0; v < values.size(); ++v)
        releaseAt[values[v].lastUse].push_back(v);

    Pools pools;
    ScratchPlan plan;
    plan.snapshots.resize(timeline.getSnapshotCount());

    auto allocationOf = [&](size_t v)
    {
        ValueAllocation allocation;
        allocation.slot = values[v].slot;
        allocation.definedAt = values[v].definedAt;
        allocation.lastUse = values[v].lastUse;
        return allocation;
    };

    auto releaseAfter = [&](size_t s)
    {
        for (size_t v : releaseAt[s])
            pools.release(values[v].slot);
    };

    step = 0;
    size_t effectIndex = 0;
    for (ScratchTimeline::Item const& item : items)
    {
        if (!item.effect)
        {
            size_t const v = snapshotValues[item.snapshot];
            values[v].slot = pools.allocate(values[v].key);
            plan.snapshots[item.snapshot] = allocationOf(v);
            releaseAfter(step);
            ++step;
            continue;
        }

        std::vector<Pass> const& passes = item.effect->getPasses();
        std::vector<size_t> const& outputs = passValues[effectIndex++];

        EffectAllocation effect;
        for (size_t i = 0; i < passes.size(); ++i)
        {
            Pass const& pass = passes[i];

            PassAllocation allocation;
            allocation.step = step;

            // The output takes its texture while every input still holds
            // its own, so the two never alias.
            if (outputs[i] != SIZE_MAX)
            {
                Value& value = values[outputs[i]];
                value.slot = pools.allocate(value.key);
                allocation.output = allocationOf(outputs[i]);
            }

            for (PassInput const& input : pass.inputs)
            {
                size_t const v = input.isBackdrop()
                    ? snapshotValues[item.snapshot]
                    : outputs[input.getPass()];
                allocation.inputs.push_back(values[v].slot);
            }

            releaseAfter(step);
            ++step;

            effect.passes.push_back(std::move(allocation));
        }

        plan.effects.push_back(std::move(effect));
    }

    plan.pools = pools.sizes();

    return plan;
}

} // namespace avg
