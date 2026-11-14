#pragma once

#include "effect.h"
#include "effectplan.h"
#include "passpipelinecache.h"
#include "scratchpool.h"
#include "targetimage.h"
#include "vector.h"
#include "avgvisibility.h"

#include <ase/commandbuffer.h>
#include <ase/framebuffer.h>
#include <ase/rendercontext.h>
#include <ase/texture.h>
#include <ase/uniformblock.h>
#include <ase/uniformring.h>

#include <cstddef>
#include <optional>
#include <vector>

namespace avg
{
    /**
     * @brief The C++ image of the shader contract's PassBlock, as the
     * emitter writes it before every draw.
     *
     * The members and their std140 placement are those passInterface
     * declares; texel0 to texel3 describe the draw's inputs in order and
     * are zero past the last input.
     */
    struct AVG_EXPORT PassBlock
    {
        float targetSize[2] = { 0.0f, 0.0f };
        float targetDivisor = 1.0f;
        float reserved = 0.0f;
        float region[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float texel0[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float texel1[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float texel2[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
        float texel3[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    };

    /**
     * @brief Records the draws that run effects over one frame's main
     * target.
     *
     *     EffectEmitter emitter(context, commands, uniforms, scratch,
     *             pipelines, mainTarget);
     *     emitter.emitSnapshot(plan.snapshots[0].slot,
     *             regions.backdropRegion);
     *     emitter.emitEffect(glass, regions, plan.effects[0], triangles);
     *
     * The main target is the full-screen target the frame draws into; its
     * size is the screen size the regions and the scratch plan were made
     * for, and the caller has begun the frame on the pool, the cache and
     * the uniform ring. The recorded draws keep their order, among
     * themselves and against the draws recorded around them.
     */
    class AVG_EXPORT EffectEmitter
    {
    public:
        EffectEmitter(ase::RenderContext& context, ase::CommandBuffer& commands,
                ase::UniformRing& uniforms, ScratchPool& scratch,
                PassPipelineCache& pipelines, TargetImage& mainTarget);
        EffectEmitter(EffectEmitter const&) = delete;
        ~EffectEmitter();

        EffectEmitter& operator=(EffectEmitter const&) = delete;

        /**
         * @brief Copies region of the main target into slot, a divisor-1
         * texture in the main target's format, with one quad draw.
         *
         * Nothing is recorded for an empty region.
         */
        void emitSnapshot(ScratchSlot const& slot, PixelRect const& region);

        /**
         * @brief Draws every pass of effect: each Region pass a quad over
         * its region into its allocated slot, the Shape pass the given
         * triangles into the main target.
         *
         * regions and allocation are deriveRegions' and planScratch's
         * results for this effect at this frame's screen size; the
         * allocation's input slots name the snapshot the effect reads.
         * shapeTriangles are the effect's shape as a triangle list in
         * screen pixels. A pass with an empty region draws nothing, and an
         * effect whose Shape region is empty records nothing. Throws
         * std::invalid_argument when regions or allocation do not match
         * the effect's passes.
         */
        void emitEffect(Effect const& effect, EffectRegions const& regions,
                EffectAllocation const& allocation,
                std::vector<Vector2f> const& shapeTriangles);

        /**
         * @brief How many pass draws this emitter has recorded.
         */
        size_t getPassCount() const;

        size_t getSnapshotCount() const;

    private:
        struct Input
        {
            ase::Texture texture;
            Vector2i size;
            Divisor divisor;
        };

        void draw(PassPipeline& pipeline, ase::Framebuffer const& target,
                Vector2i targetSize, Divisor divisor, PixelRect const& region,
                std::vector<Input> const& inputs,
                std::optional<ase::UniformValues> const& parameters,
                std::vector<float> const& vertices);
        void pushBarrier();

        ase::RenderContext& context_;
        ase::CommandBuffer& commands_;
        ase::UniformRing& uniforms_;
        ScratchPool& scratch_;
        PassPipelineCache& pipelines_;
        TargetImage& mainTarget_;
        ase::UniformBlock<PassBlock> passBlock_;
        size_t passCount_ = 0;
        size_t snapshotCount_ = 0;
    };
}
