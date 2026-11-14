#pragma once

#include "effect.h"
#include "avgvisibility.h"

#include <ase/format.h>
#include <ase/pipeline.h>
#include <ase/program.h>
#include <ase/rendercontext.h>
#include <ase/shaderinterface.h>
#include <ase/uniformset.h>

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace avg
{
    /**
     * @brief A pipeline built for one pass shader, with the bindings a
     * draw of it fills.
     *
     * A draw binds its blocks by the numbers the pipeline reports. Every
     * draw of a frame takes its own uniform set from acquireUniformSet;
     * PassPipelineCache::beginFrame hands the sets out again.
     */
    class AVG_EXPORT PassPipeline
    {
    public:
        PassPipeline(PassPipeline const&) = delete;
        PassPipeline(PassPipeline&&) noexcept;
        ~PassPipeline();

        PassPipeline& operator=(PassPipeline const&) = delete;
        PassPipeline& operator=(PassPipeline&&) = delete;

        ase::Pipeline const& getPipeline() const;
        int getPassBlockBinding() const;
        std::optional<int> getParamBlockBinding() const;

        /**
         * @brief How many textures a draw of the pipeline binds.
         */
        size_t getInputCount() const;

        /**
         * @brief A uniform set no draw of the current frame has bound yet.
         */
        ase::UniformSet acquireUniformSet();

    private:
        friend class PassPipelineCache;

        PassPipeline(ase::RenderContext& context, ase::Pipeline pipeline,
                ase::ShaderInterface iface);

        void beginFrame();

        ase::RenderContext& context_;
        ase::Pipeline pipeline_;
        ase::ShaderInterface interface_;
        int passBlockBinding_ = 0;
        std::optional<int> paramBlockBinding_;
        size_t inputCount_ = 0;
        std::vector<ase::UniformSet> uniformSets_;
        size_t usedSets_ = 0;
    };

    /**
     * @brief The pipelines effect passes draw with, built on first use and
     * kept for the window's lifetime.
     *
     *     cache.beginFrame();
     *     PassPipeline& pipeline = cache.getPipeline(pass.shader,
     *             pass.geometry, colorFormat, depthFormat);
     *
     * The same fragment source, geometry and target colour and depth
     * formats give the same pipeline, so an effect whose parameters change
     * keeps its pipelines. A Region pipeline overwrites its target; a Shape
     * one blends source alpha over the main target.
     */
    class AVG_EXPORT PassPipelineCache
    {
    public:
        explicit PassPipelineCache(ase::RenderContext& context);
        PassPipelineCache(PassPipelineCache const&) = delete;
        PassPipelineCache(PassPipelineCache&&) noexcept;
        ~PassPipelineCache();

        PassPipelineCache& operator=(PassPipelineCache const&) = delete;
        PassPipelineCache& operator=(PassPipelineCache&&) = delete;

        /**
         * @brief Returns every pipeline's uniform sets to use.
         */
        void beginFrame();

        PassPipeline& getPipeline(PassShader const& shader, Geometry geometry,
                ase::Format colorFormat,
                std::optional<ase::Format> depthFormat);

        size_t getPipelineCount() const;

    private:
        struct PipelineKey
        {
            ase::Program program;
            Geometry geometry;
            ase::Format colorFormat;
            std::optional<ase::Format> depthFormat;

            bool operator<(PipelineKey const& rhs) const;
        };

        ase::Program const& getProgram(PassShader const& shader);

        ase::RenderContext& context_;
        std::map<std::string, ase::Program> programs_;
        std::map<PipelineKey, PassPipeline> pipelines_;
    };
}
