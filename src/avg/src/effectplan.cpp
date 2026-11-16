#include "effectplan.h"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

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

PassGraph::PassGraph(Vector2i screenSize) :
    screenSize_(screenSize)
{
}

PassGraph::~PassGraph()
{
}

size_t PassGraph::addSnapshot()
{
    Item item;
    item.snapshot = backdropRegions_.size();
    items_.push_back(item);
    backdropRegions_.push_back(PixelRect());
    return item.snapshot;
}

size_t PassGraph::addEffect(Effect const& effect, size_t snapshot,
        PixelRect shapeBounds)
{
    if (snapshot >= backdropRegions_.size())
    {
        throw std::invalid_argument("effect reads snapshot "
                + std::to_string(snapshot) + ", which has not been taken");
    }

    size_t const effectIndex = effectCount_++;
    EffectRegions const regions = deriveRegions(effect, shapeBounds,
            screenSize_);
    backdropRegions_[snapshot] = backdropRegions_[snapshot].unionWith(
            regions.backdropRegion);

    std::vector<Pass> const& passes = effect.getPasses();

    elementNodes_.push_back(std::nullopt);

    Item item;
    item.kind = Kind::Effect;
    item.effect = &effect;
    item.snapshot = snapshot;

    for (size_t i = 0; i < passes.size(); ++i)
    {
        Pass const& pass = passes[i];

        Node node;
        node.pass = &pass;
        node.snapshot = snapshot;
        node.region = regions.passRegions[i];
        node.effect = effectIndex;
        node.index = i;
        for (PassInput const& input : pass.inputs)
        {
            node.inputs.push_back(input.isBackdrop()
                    ? std::nullopt
                    : std::optional<size_t>(item.nodes[input.getPass()]));
        }

        auto same = std::find_if(nodes_.begin(), nodes_.end(),
                [&](Node const& other)
                {
                    return pass.output
                        && !other.subDrawing
                        && other.snapshot == node.snapshot
                        && other.inputs == node.inputs
                        && *other.pass == pass;
                });

        if (same == nodes_.end())
        {
            item.nodes.push_back(nodes_.size());
            nodes_.push_back(std::move(node));
        }
        else
        {
            same->region = same->region.unionWith(node.region);
            item.nodes.push_back(
                    static_cast<size_t>(same - nodes_.begin()));
        }
    }

    items_.push_back(std::move(item));
    return effectIndex;
}

size_t PassGraph::addSubDrawing()
{
    size_t const index = subDrawingNodes_.size();

    Node node;
    node.index = index;
    node.subDrawing = true;

    Item item;
    item.kind = Kind::SubDrawing;
    item.nodes.push_back(nodes_.size());
    items_.push_back(std::move(item));

    subDrawingNodes_.push_back(nodes_.size());
    nodes_.push_back(std::move(node));
    return index;
}

size_t PassGraph::addElement(Pass const& pass, size_t snapshot,
        std::vector<Source> const& inputs, PixelRect region)
{
    Node node;
    node.pass = &pass;
    node.snapshot = snapshot;
    node.effect = effectCount_;
    node.index = 0;

    for (Source const& source : inputs)
    {
        switch (source.kind)
        {
            case Source::Kind::Backdrop:
                if (snapshot >= backdropRegions_.size())
                {
                    throw std::invalid_argument("element reads snapshot "
                            + std::to_string(snapshot)
                            + ", which has not been taken");
                }
                node.inputs.push_back(std::nullopt);
                break;
            case Source::Kind::SubDrawing:
                if (source.index >= subDrawingNodes_.size())
                {
                    throw std::invalid_argument("element reads sub-drawing "
                            + std::to_string(source.index)
                            + ", which has not been added");
                }
                node.inputs.push_back(subDrawingNodes_[source.index]);
                break;
            case Source::Kind::Element:
                if (source.index >= elementNodes_.size()
                        || !elementNodes_[source.index])
                {
                    throw std::invalid_argument("element reads element "
                            + std::to_string(source.index)
                            + ", which has not been added");
                }
                node.inputs.push_back(*elementNodes_[source.index]);
                break;
        }
    }

    Divisor const divisor = divisorOf(pass);
    node.region = region.clamped(targetSize(screenSize_, divisor));

    size_t const effectIndex = effectCount_++;

    auto same = std::find_if(nodes_.begin(), nodes_.end(),
            [&](Node const& other)
            {
                return pass.output
                    && !other.subDrawing
                    && other.snapshot == node.snapshot
                    && other.inputs == node.inputs
                    && *other.pass == pass;
            });

    size_t n = static_cast<size_t>(same - nodes_.begin());
    PixelRect const added = node.region;
    if (same == nodes_.end())
    {
        node.region = PixelRect();
        nodes_.push_back(std::move(node));
    }

    grow(n, added, divisor);

    elementNodes_.push_back(n);

    Item item;
    item.kind = Kind::Element;
    item.snapshot = snapshot;
    item.nodes.push_back(n);
    items_.push_back(std::move(item));

    return effectIndex;
}

void PassGraph::grow(size_t n, PixelRect region, Divisor divisor)
{
    Node& node = nodes_[n];
    Divisor const own = node.pass ? divisorOf(*node.pass) : Divisor::One;
    PixelRect const added = region.rescaled(divisor, own)
        .clamped(targetSize(screenSize_, own));

    if (added.isEmpty() || node.region.unionWith(added) == node.region)
        return;

    node.region = node.region.unionWith(added);

    if (!node.pass)
        return;

    PixelRect const sampled = added.grown(node.pass->padding);
    std::vector<std::optional<size_t>> const inputs = node.inputs;
    size_t const snapshot = node.snapshot;
    for (std::optional<size_t> const& input : inputs)
    {
        if (input)
            grow(*input, sampled, own);
        else
            growBackdrop(snapshot, sampled, own);
    }
}

void PassGraph::growBackdrop(size_t snapshot, PixelRect region,
        Divisor divisor)
{
    backdropRegions_[snapshot] = backdropRegions_[snapshot].unionWith(
            region.rescaled(divisor, Divisor::One).clamped(screenSize_));
}

PixelRect PassGraph::getBackdropRegion(size_t snapshot) const
{
    return backdropRegions_.at(snapshot);
}

Vector2i PassGraph::getScreenSize() const
{
    return screenSize_;
}

size_t PassGraph::getSnapshotCount() const
{
    return backdropRegions_.size();
}

size_t PassGraph::getEffectCount() const
{
    return effectCount_;
}

size_t PassGraph::getSubDrawingCount() const
{
    return subDrawingNodes_.size();
}

std::vector<PassGraph::Node> const& PassGraph::getNodes() const
{
    return nodes_;
}

std::vector<PassGraph::Item> const& PassGraph::getItems() const
{
    return items_;
}

namespace
{
    ScratchKey snapshotKey()
    {
        return ScratchKey();
    }

    ScratchKey subDrawingKey()
    {
        return ScratchKey{ Divisor::One, ase::FORMAT_SRGBA };
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

ScratchPlan planScratch(PassGraph const& graph)
{
    std::vector<PassGraph::Item> const& items = graph.getItems();
    std::vector<PassGraph::Node> const& nodes = graph.getNodes();

    std::vector<Value> values;
    std::vector<size_t> snapshotValues(graph.getSnapshotCount());
    // Per node: the value index of its output, or SIZE_MAX.
    std::vector<size_t> nodeValues(nodes.size(), SIZE_MAX);
    std::vector<size_t> nodeSteps(nodes.size());

    auto runs = [&](PassGraph::Item const& item, size_t i, size_t effect)
    {
        PassGraph::Node const& node = nodes[item.nodes[i]];
        return node.effect == effect && node.index == i;
    };

    auto valueOf = [&](PassGraph::Node const& node, size_t input)
    {
        std::optional<size_t> const& source = node.inputs[input];
        return source ? nodeValues[*source] : snapshotValues[node.snapshot];
    };

    size_t step = 0;
    size_t effect = 0;
    for (PassGraph::Item const& item : items)
    {
        if (item.kind == PassGraph::Kind::Snapshot)
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

        if (item.kind == PassGraph::Kind::SubDrawing)
        {
            size_t const n = item.nodes.front();
            Value value;
            value.key = subDrawingKey();
            value.definedAt = step;
            value.lastUse = step;
            nodeValues[n] = values.size();
            values.push_back(value);
            nodeSteps[n] = step;
            ++step;
            continue;
        }

        for (size_t i = 0; i < item.nodes.size(); ++i)
        {
            if (!runs(item, i, effect))
                continue;

            size_t const n = item.nodes[i];
            PassGraph::Node const& node = nodes[n];

            for (size_t input = 0; input < node.inputs.size(); ++input)
            {
                Value& read = values[valueOf(node, input)];
                read.lastUse = std::max(read.lastUse, step);
            }

            if (node.pass->output)
            {
                Value value;
                value.key = keyOf(*node.pass->output);
                value.definedAt = step;
                value.lastUse = step;
                nodeValues[n] = values.size();
                values.push_back(value);
            }

            nodeSteps[n] = step;
            ++step;
        }

        ++effect;
    }

    std::vector<std::vector<size_t>> releaseAt(step);
    for (size_t v = 0; v < values.size(); ++v)
        releaseAt[values[v].lastUse].push_back(v);

    Pools pools;
    ScratchPlan plan;
    plan.snapshots.resize(graph.getSnapshotCount());

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

    effect = 0;
    for (PassGraph::Item const& item : items)
    {
        if (item.kind == PassGraph::Kind::Snapshot)
        {
            size_t const v = snapshotValues[item.snapshot];
            values[v].slot = pools.allocate(values[v].key);
            plan.snapshots[item.snapshot] = allocationOf(v);
            releaseAfter(values[v].definedAt);
            continue;
        }

        if (item.kind == PassGraph::Kind::SubDrawing)
        {
            size_t const v = nodeValues[item.nodes.front()];
            values[v].slot = pools.allocate(values[v].key);
            plan.subDrawings.push_back(allocationOf(v));
            releaseAfter(values[v].definedAt);
            continue;
        }

        EffectAllocation allocation;
        for (size_t i = 0; i < item.nodes.size(); ++i)
        {
            if (!runs(item, i, effect))
                continue;

            size_t const n = item.nodes[i];
            PassGraph::Node const& node = nodes[n];

            PassAllocation pass;
            pass.pass = i;
            pass.step = nodeSteps[n];
            pass.region = node.region;

            // The output takes its texture while every input still holds
            // its own, so the two never alias.
            if (nodeValues[n] != SIZE_MAX)
            {
                Value& value = values[nodeValues[n]];
                value.slot = pools.allocate(value.key);
                pass.output = allocationOf(nodeValues[n]);
            }

            for (size_t input = 0; input < node.inputs.size(); ++input)
                pass.inputs.push_back(values[valueOf(node, input)].slot);

            releaseAfter(pass.step);

            allocation.passes.push_back(std::move(pass));
        }

        plan.effects.push_back(std::move(allocation));
        ++effect;
    }

    plan.pools = pools.sizes();

    return plan;
}

} // namespace avg
