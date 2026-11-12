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
     * Steps number the timeline's snapshots and passes from zero in
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
     * @brief The scratch textures one pass reads and writes.
     *
     * inputs line up with the pass's inputs; output is absent for the
     * final pass.
     */
    struct AVG_EXPORT PassAllocation
    {
        size_t step = 0;
        std::vector<ScratchSlot> inputs;
        std::optional<ValueAllocation> output;
    };

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
     * of a ScratchTimeline.
     *
     * snapshots and effects are indexed as the timeline numbered them.
     * pools lists every key in use with the texture count it needs; the
     * count for an unused key is zero.
     */
    struct AVG_EXPORT ScratchPlan
    {
        std::vector<ValueAllocation> snapshots;
        std::vector<EffectAllocation> effects;
        std::vector<PoolSize> pools;

        size_t getPoolSize(ScratchKey const& key) const;
    };

    /**
     * @brief The scope snapshots and effects of a frame in execution order.
     *
     *     ScratchTimeline timeline;
     *     size_t root = timeline.addSnapshot();
     *     timeline.addEffect(glass, root);
     *
     * A snapshot is taken when its scope is entered; an effect runs where
     * it is added and reads the snapshot it names as its backdrop. The
     * timeline keeps pointers to the effects, which must outlive it.
     */
    class AVG_EXPORT ScratchTimeline
    {
    public:
        /**
         * @brief One addition: a snapshot when effect is null, else an
         * effect. snapshot is the snapshot's own index for the former and
         * the backdrop the effect reads for the latter.
         */
        struct Item
        {
            Effect const* effect = nullptr;
            size_t snapshot = 0;
        };

        ScratchTimeline();
        ScratchTimeline(ScratchTimeline const&) = default;
        ScratchTimeline(ScratchTimeline&&) noexcept = default;
        ~ScratchTimeline();

        ScratchTimeline& operator=(ScratchTimeline const&) = default;
        ScratchTimeline& operator=(ScratchTimeline&&) noexcept = default;

        /**
         * @brief Takes a snapshot at this point; returns its index.
         */
        size_t addSnapshot();

        /**
         * @brief Runs effect at this point with snapshot as its backdrop;
         * returns the effect's index. Throws std::invalid_argument when
         * snapshot has not been added.
         */
        size_t addEffect(Effect const& effect, size_t snapshot);

        size_t getSnapshotCount() const;
        size_t getEffectCount() const;
        std::vector<Item> const& getItems() const;

    private:
        size_t snapshotCount_ = 0;
        size_t effectCount_ = 0;
        std::vector<Item> items_;
    };

    /**
     * @brief Assigns every snapshot and pass output of the timeline a
     * scratch texture, reusing a texture once its last reader has run.
     *
     * A pass's output never shares a texture with one of its inputs, so
     * no pass samples the texture it writes.
     */
    AVG_EXPORT ScratchPlan planScratch(ScratchTimeline const& timeline);
}
