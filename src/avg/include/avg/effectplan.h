#pragma once

#include "effect.h"
#include "vector.h"
#include "avgvisibility.h"

#include <ase/format.h>

#include <cstddef>
#include <optional>
#include <ostream>
#include <vector>

namespace avg
{
    /**
     * @brief An integer pixel rectangle in a target's own pixels.
     *
     * It is empty when width or height is not positive; every operation
     * returns the all-zero rectangle for an empty result.
     */
    struct AVG_EXPORT PixelRect
    {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;

        bool isEmpty() const;

        /**
         * @brief The smallest rectangle containing both; an empty side
         * contributes nothing.
         */
        PixelRect unionWith(PixelRect const& rhs) const;

        /**
         * @brief Grows every edge outwards by amount pixels.
         */
        PixelRect grown(int amount) const;

        /**
         * @brief The part inside [0, size), empty when nothing is.
         */
        PixelRect clamped(Vector2i size) const;

        /**
         * @brief Rescales from a target at divisor from to one at divisor
         * to, rounding outwards so every covered pixel stays covered.
         */
        PixelRect rescaled(Divisor from, Divisor to) const;

        bool operator==(PixelRect const& rhs) const;
        bool operator!=(PixelRect const& rhs) const;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            PixelRect const& r);

    /**
     * @brief The size of a target at a divisor of the screen: the screen
     * size divided by the divisor and rounded up.
     */
    AVG_EXPORT Vector2i targetSize(Vector2i screenSize, Divisor divisor);

    /**
     * @brief Every pass's output region for an effect drawn at a given
     * place on the screen.
     *
     * passRegions[i] is in the pixels of pass i's own target, so the final
     * pass's region is in screen pixels. backdropRegion, in screen pixels,
     * is what every backdrop-reading pass of the effect needs from the
     * snapshot, and is empty when no pass reads the backdrop.
     */
    struct AVG_EXPORT EffectRegions
    {
        std::vector<PixelRect> passRegions;
        PixelRect backdropRegion;
    };

    /**
     * @brief Computes the region each pass writes when the effect's shape
     * covers shapeBounds (in screen pixels) of a screen of screenSize.
     *
     *     auto regions = deriveRegions(blur, { 100, 80, 200, 40 },
     *             Vector2i(1920, 1080));
     *
     * The final pass writes the shape's bounds clamped to the screen; every
     * other pass writes what its readers sample, grown by each reader's
     * padding, at its own divisor and clamped to its own target.
     */
    AVG_EXPORT EffectRegions deriveRegions(Effect const& effect,
            PixelRect shapeBounds, Vector2i screenSize);

    /**
     * @brief Which pool of scratch textures a value lives in.
     */
    struct AVG_EXPORT ScratchKey
    {
        Divisor divisor = Divisor::One;
        ase::Format format = ase::FORMAT_SRGBA;

        bool operator==(ScratchKey const& rhs) const;
        bool operator!=(ScratchKey const& rhs) const;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            ScratchKey const& key);

    /**
     * @brief One scratch texture: the index-th texture of its key's pool.
     */
    struct AVG_EXPORT ScratchSlot
    {
        ScratchKey key;
        size_t index = 0;

        bool operator==(ScratchSlot const& rhs) const;
        bool operator!=(ScratchSlot const& rhs) const;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream,
            ScratchSlot const& slot);

    /**
     * @brief Where a value that lives in scratch is, and for which steps.
     *
     * Steps number the graph's snapshots and nodes from zero in
     * execution order. The slot is written at definedAt and may be handed
     * to another value once the step at lastUse has executed; a value
     * nobody reads has lastUse equal to definedAt.
     */
    struct AVG_EXPORT ValueAllocation
    {
        ScratchSlot slot;
        size_t definedAt = 0;
        size_t lastUse = 0;
    };

    /**
     * @brief One pass an effect runs: where it draws and the scratch
     * textures it reads and writes.
     *
     * pass indexes the effect's passes and region is in the pixels of its
     * own target. inputs line up with the pass's inputs; output is absent
     * for the final pass.
     */
    struct AVG_EXPORT PassAllocation
    {
        size_t pass = 0;
        size_t step = 0;
        PixelRect region;
        std::vector<ScratchSlot> inputs;
        std::optional<ValueAllocation> output;
    };

    /**
     * @brief The passes an effect runs, in order, ending with its final
     * pass; a pass merged with one an earlier effect runs is left out.
     */
    struct AVG_EXPORT EffectAllocation
    {
        std::vector<PassAllocation> passes;
    };

    /**
     * @brief How many textures a pool needs.
     */
    struct AVG_EXPORT PoolSize
    {
        ScratchKey key;
        size_t count = 0;
    };

    /**
     * @brief What a frame's scopes and effects do to scratch, in the order
     * of a PassGraph.
     *
     * snapshots, subDrawings and effects are indexed as the graph
     * numbered them; an element counts as an effect of one pass.
     * pools lists every key in use with the texture count it needs; the
     * count for an unused key is zero.
     */
    struct AVG_EXPORT ScratchPlan
    {
        std::vector<ValueAllocation> snapshots;
        std::vector<ValueAllocation> subDrawings;
        std::vector<EffectAllocation> effects;
        std::vector<PoolSize> pools;

        size_t getPoolSize(ScratchKey const& key) const;
    };

    /**
     * @brief The scope snapshots, effects, sub-drawings and effect element
     * passes of a frame in execution order, lowered into one graph of
     * passes.
     *
     *     PassGraph graph(Vector2i(1920, 1080));
     *     size_t root = graph.addSnapshot();
     *     graph.addEffect(glass, root, { 100, 80, 200, 40 });
     *
     * A snapshot is taken when its scope is entered; an effect runs where
     * it is added and reads the snapshot it names as its backdrop. A Region
     * pass equal to one already added over the same snapshot, reading the
     * same snapshot and nodes, is merged into that node: it runs once,
     * where it first ran, over the bounding box of every region it was
     * added with. Final passes and sub-drawings are never merged, so a
     * merged pass must not depend on the region it covers. A sub-drawing
     * is rendered into a target of its own where it is added; an element
     * pass reads sub-drawings, earlier element passes and the snapshot, and
     * every source it reads grows to what the pass samples, through the
     * sources that source reads in turn. The graph keeps pointers to the
     * passes, which must outlive it.
     */
    class AVG_EXPORT PassGraph
    {
    public:
        /**
         * @brief A pass of the graph, or a sub-drawing.
         *
         * An input is a node index, or empty for the snapshot. effect and
         * index name the effect or element that runs the pass and the
         * pass's index there; for a sub-drawing, pass is null and index is
         * the sub-drawing's own.
         */
        struct Node
        {
            Pass const* pass = nullptr;
            size_t snapshot = 0;
            std::vector<std::optional<size_t>> inputs;
            PixelRect region;
            size_t effect = 0;
            size_t index = 0;
            bool subDrawing = false;
        };

        enum class Kind
        {
            Snapshot,
            Effect,
            Element,
            SubDrawing
        };

        /**
         * @brief One addition. snapshot is the snapshot's own index for a
         * snapshot and the backdrop read for an effect or an element;
         * nodes holds the node each pass was lowered to, or the
         * sub-drawing's node. effect is set for an effect only.
         */
        struct Item
        {
            Kind kind = Kind::Snapshot;
            Effect const* effect = nullptr;
            size_t snapshot = 0;
            std::vector<size_t> nodes;
        };

        /**
         * @brief What an element pass reads: the snapshot, a sub-drawing
         * or an earlier element pass, by the index its add returned. An
         * element cannot read an effect.
         */
        struct Source
        {
            Kind kind = Kind::Snapshot;
            size_t index = 0;
        };

        explicit PassGraph(Vector2i screenSize);
        PassGraph(PassGraph const&) = default;
        PassGraph(PassGraph&&) noexcept = default;
        ~PassGraph();

        PassGraph& operator=(PassGraph const&) = default;
        PassGraph& operator=(PassGraph&&) noexcept = default;

        /**
         * @brief Takes a snapshot at this point; returns its index.
         */
        size_t addSnapshot();

        /**
         * @brief Runs effect at this point with snapshot as its backdrop
         * and its shape over shapeBounds, in screen pixels; returns the
         * effect's index. Throws std::invalid_argument when snapshot has
         * not been added.
         */
        size_t addEffect(Effect const& effect, size_t snapshot,
                PixelRect shapeBounds);

        /**
         * @brief Renders a sub-drawing into a divisor-1 FORMAT_SRGBA target
         * at this point; returns its index. Its region grows with what the
         * element passes reading it sample.
         */
        size_t addSubDrawing();

        /**
         * @brief Runs one element pass at this point over inputs, which
         * line up with pass.inputs; returns its index among the effects.
         *
         * A Shape pass draws region, in screen pixels, into the target the
         * caller draws into and is never merged. A Region pass starts from
         * region, at its output's divisor, and grows as later passes read
         * it. snapshot is only read when an input is the backdrop. Throws
         * std::invalid_argument when a source has not been added.
         */
        size_t addElement(Pass const& pass, size_t snapshot,
                std::vector<Source> const& inputs, PixelRect region);

        /**
         * @brief The part of the screen, in screen pixels, that the passes
         * reading snapshot need from it; empty when none do.
         */
        PixelRect getBackdropRegion(size_t snapshot) const;

        Vector2i getScreenSize() const;
        size_t getSnapshotCount() const;
        size_t getEffectCount() const;
        size_t getSubDrawingCount() const;
        std::vector<Node> const& getNodes() const;
        std::vector<Item> const& getItems() const;

    private:
        size_t findMergeable(Node const& node) const;
        void grow(size_t nodeIndex, PixelRect region, Divisor divisor);
        void growBackdrop(size_t snapshot, PixelRect region, Divisor divisor);

        Vector2i screenSize_;
        size_t effectCount_ = 0;
        std::vector<size_t> subDrawingNodes_;
        // Per effect index, its node when it is an element.
        std::vector<std::optional<size_t>> elementNodes_;
        std::vector<PixelRect> backdropRegions_;
        std::vector<Node> nodes_;
        std::vector<Item> items_;
    };

    /**
     * @brief Assigns every snapshot and node output of the graph a scratch
     * texture, reusing a texture once its last reader has run.
     *
     * A pass's output never shares a texture with one of its inputs, so
     * no pass samples the texture it writes.
     */
    AVG_EXPORT ScratchPlan planScratch(PassGraph const& graph);
}
