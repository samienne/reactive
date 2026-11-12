#pragma once

#include "blendmode.h"
#include "format.h"

#include "asevisibility.h"

#include <optional>

namespace ase
{
    /**
     * @brief The target formats and fixed-function state a pipeline draws
     * with.
     *
     * The formats are those of the framebuffer the pipeline will draw into
     * and are given at construction: `PipelineDesc{ FORMAT_SRGBA }` is an
     * opaque pipeline into a colour-only target that tests and writes
     * depth. Set `blend` for a blended one; it then still tests depth but
     * does not write it unless `depthWrite` says otherwise. An effect pass
     * that ignores depth sets `depthTest = false`; such a draw is never
     * reordered, so it covers what was pushed before it and is covered by
     * what is pushed after.
     */
    struct ASE_EXPORT PipelineDesc
    {
        /**
         * @brief `depthFormat` unset means the target has no depth
         * attachment.
         */
        explicit PipelineDesc(Format colorFormat,
                std::optional<Format> depthFormat = std::nullopt);

        Format colorFormat;
        std::optional<Format> depthFormat;
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
