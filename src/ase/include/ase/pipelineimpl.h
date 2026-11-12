#pragma once

#include "blendmode.h"
#include "format.h"
#include "program.h"
#include "vertexspec.h"

#include "asevisibility.h"

#include <optional>

namespace ase
{
    class RenderContext;

    class ASE_EXPORT PipelineImpl
    {
    public:
        virtual ~PipelineImpl() = default;
        virtual Program const& getProgram() const = 0;
        virtual VertexSpec const& getVertexSpec() const = 0;
        virtual Format getColorFormat() const = 0;
        virtual std::optional<Format> getDepthFormat() const = 0;
        virtual bool isBlendEnabled() const = 0;
        virtual bool isDepthTestEnabled() const = 0;
        virtual bool isDepthWriteEnabled() const = 0;
    };
} // namespace ase
