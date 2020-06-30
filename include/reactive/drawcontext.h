#pragma once

#include "reactivevisibility.h"

#include <avg/drawing.h>
#include <avg/pathbuilder.h>
#include <avg/painter.h>

#include <pmr/memory_resource.h>

namespace reactive
{
    class REACTIVE_EXPORT DrawContext
    {
    public:
        explicit DrawContext(std::shared_ptr<avg::Painter> painter);
        DrawContext(DrawContext const& rhs) = default;
        DrawContext(DrawContext&& rhs) noexcept = default;

        DrawContext& operator=(DrawContext const& rhs) = default;
        DrawContext& operator=(DrawContext&& rhs) noexcept = default;

        bool operator==(DrawContext const& other) const;

        pmr::memory_resource* getResource() const;

        avg::PathBuilder pathBuilder() const;

        template <typename... Ts>
        avg::Drawing drawing(Ts&&... ts) const
        {
            return avg::Drawing(getResource(), std::forward<Ts>(ts)...);
        }

    private:
        std::shared_ptr<avg::Painter> painter_;
    };
} // namespace reactive


