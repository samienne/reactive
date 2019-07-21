#pragma once

#include "reactivevisibility.h"

#include <avg/pathbuilder.h>

#include <pmr/memory_resource.h>

namespace reactive
{
    class REACTIVE_EXPORT DrawContext
    {
    public:
        explicit DrawContext(pmr::memory_resource* memory);
        DrawContext(DrawContext const& rhs) = default;
        DrawContext(DrawContext& rhs) noexcept = default;

        DrawContext& operator=(DrawContext const& rhs) = default;
        DrawContext& operator=(DrawContext& rhs) noexcept = default;

        pmr::memory_resource* getResource() const;

        avg::PathBuilder pathBuilder() const;

    private:
        pmr::memory_resource* memory_;
    };
} // namespace reactive


