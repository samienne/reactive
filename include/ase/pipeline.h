#pragma once

#include "pipelineimpl.h"
#include "blendmode.h"
#include "program.h"
#include "vertexspec.h"

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class NamedVertexSpec;
    class Program;
    class RenderContext;

    class BTL_VISIBLE Pipeline
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

        /*
        VertexSpec const& getSpec() const;
        bool isBlendEnabled() const;
        BlendMode getSrcFactor() const;
        BlendMode getDstFactor() const;
        */

        template <class T>
        inline T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    protected:
        inline PipelineImpl* d() { return deferred_.get(); }
        inline PipelineImpl const* d() const { return deferred_.get(); }

        std::shared_ptr<PipelineImpl> deferred_;
    };
}

