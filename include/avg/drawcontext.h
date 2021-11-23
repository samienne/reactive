#pragma once

#include "drawing.h"
#include "pathbuilder.h"
#include "painter.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>

namespace avg
{
    class AVG_EXPORT DrawContext
    {
    public:
        explicit DrawContext(avg::Painter* painter);
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
        avg::Painter* painter_;
    };
} // namespace reactive

