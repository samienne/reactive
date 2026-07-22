#pragma once

#include "drawing.h"
#include "pathbuilder.h"
#include "avgvisibility.h"

#include <pmr/memory_resource.h>

namespace avg
{
    class Painter;

    /**
     * @brief The memory a draw allocates its result out of.
     *
     * Drawing a render tree produces paths, shapes and drawings and nothing
     * else, so memory is all a node ever asks of its context.
     */
    class AVG_EXPORT DrawContext
    {
    public:
        /**
         * @brief Allocates out of the memory @p painter paints out of.
         */
        explicit DrawContext(avg::Painter* painter);

        /**
         * @brief Allocates out of @p memory alone, with no rendering back end.
         *
         * A tree drawn this way yields the same avg::Drawing it would on
         * screen, so drawing can be exercised where no graphics context
         * exists.
         */
        explicit DrawContext(pmr::memory_resource* memory);

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
        pmr::memory_resource* memory_;
    };
} // namespace avg

