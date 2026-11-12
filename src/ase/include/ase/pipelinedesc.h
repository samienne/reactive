#pragma once

#include "blendmode.h"

#include "asevisibility.h"

#include <optional>

namespace ase
{
    /**
     * @brief The fixed-function state a pipeline draws with.
     *
     * The default is an opaque pipeline that tests and writes depth, as
     * `makePipeline(program, spec)` gives. Set `blend` for a blended one;
     * it then leaves depth alone unless `depthWrite` says otherwise. An
     * effect pass that ignores depth sets `depthTest = false`.
     */
    struct ASE_EXPORT PipelineDesc
    {
        std::optional<BlendFunc> blend;
        bool depthTest = true;

        /**
         * @brief Unset means on for an opaque pipeline and off for a
         * blended one.
         */
        std::optional<bool> depthWrite;

        bool writesDepth() const;
    };
} // namespace ase
