#pragma once

#include "blendmode.h"
#include "program.h"
#include "vertexspec.h"

#include "asevisibility.h"

namespace ase
{
    class RenderContext;

    class ASE_EXPORT PipelineImpl
    {
    public:
        virtual ~PipelineImpl() = default;
        virtual Program const& getProgram() const = 0;
    };
} // namespace ase

