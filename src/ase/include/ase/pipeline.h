#pragma once

#include "pipelineimpl.h"
#include "blendmode.h"
#include "format.h"
#include "program.h"
#include "vertexspec.h"

#include "asevisibility.h"

#include <memory>
#include <optional>

namespace ase
{
    class Framebuffer;
    class RenderContext;

    class ASE_EXPORT Pipeline
    {
    public:
        Pipeline(std::shared_ptr<PipelineImpl> impl);

        Pipeline(Pipeline const& rhs) = default;
        Pipeline(Pipeline&& rhs) noexcept = default;

        Pipeline& operator=(Pipeline const& rhs) = default;
        Pipeline& operator=(Pipeline&& rhs) noexcept = default;

        bool operator==(Pipeline const& rhs) const;
        bool operator!=(Pipeline const& rhs) const;
        bool operator<(Pipeline const& rhs) const;

        Program const& getProgram() const;
        VertexSpec const& getVertexSpec() const;
        Format getColorFormat() const;
        std::optional<Format> getDepthFormat() const;
        bool isBlendEnabled() const;
        bool isDepthTestEnabled() const;
        bool isDepthWriteEnabled() const;

        /**
         * @brief Whether `target`'s attachments have the formats this
         * pipeline declared.
         *
         * A depth attachment the target reports as `FORMAT_UNKNOWN` is
         * present but of a format the backend cannot tell, and matches any
         * declared depth format.
         */
        bool matchesTarget(Framebuffer const& target) const;

        template <class T>
        inline T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    public:
        inline PipelineImpl* d() { return deferred_.get(); }
        inline PipelineImpl const* d() const { return deferred_.get(); }

        std::shared_ptr<PipelineImpl> deferred_;
    };
}

