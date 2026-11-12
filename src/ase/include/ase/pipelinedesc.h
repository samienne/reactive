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
     * it then still tests depth but does not write it unless `depthWrite`
     * says otherwise. An effect pass that ignores depth sets
     * `depthTest = false`; such a draw is never reordered, so it covers
     * what was pushed before it and is covered by what is pushed after.
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
