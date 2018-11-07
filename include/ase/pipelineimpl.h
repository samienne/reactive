#pragma once

#include "blendmode.h"
#include "program.h"
#include "vertexspec.h"

#include <btl/visibility.h>

namespace ase
{
    class RenderContext;

    class BTL_VISIBLE PipelineImpl
    {
    public:
        virtual ~PipelineImpl() = default;
        virtual Program const& getProgram() const = 0;
    };
} // namespace ase

