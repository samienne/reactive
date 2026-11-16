#pragma once

#include "color.h"
#include "effect.h"
#include "transform.h"
#include "avgvisibility.h"

#include <btl/hash.h>

#include <ostream>
#include <variant>

namespace avg
{
    /**
     * @brief What a fill or a stroke paints with: a colour, or an effect
     * that paints through the geometry.
     *
     *     Brush red(Color(1.0f, 0.0f, 0.0f));
     *     Brush glass(frostedGlass());
     *
     * An effect brush reads the backdrop of the nearest enclosing backdrop
     * scope, not what was painted before it inside that scope.
     */
    class AVG_EXPORT Brush final
    {
    public:
        Brush(Color const& color = Color());
        Brush(Effect effect);
        Brush(Brush const&) = default;
        Brush(Brush&&) noexcept = default;
        ~Brush();

        Brush& operator=(Brush const&) = default;
        Brush& operator=(Brush&&) noexcept = default;

        /**
         * @brief The colour of a colour brush; only valid when getEffect()
         * is null.
         */
        Color const& getColor() const noexcept;

        /**
         * @brief The effect of an effect brush, or null for a colour brush.
         */
        Effect const* getEffect() const noexcept;

        Brush operator*(float scale) const noexcept;

        bool operator==(Brush const& rhs) const noexcept;
        bool operator!=(Brush const& rhs) const noexcept;

        /**
         * @brief Orders colour brushes by colour and every one before every
         * effect brush; effect brushes are not ordered among themselves.
         */
        bool operator<(Brush const& rhs) const noexcept;
        bool operator>(Brush const& rhs) const noexcept;

        friend Brush operator*(Transform const&, Brush const& b) noexcept
        {
            return b;
        }

        template <class THash>
        friend void hash_append(THash& h, Brush const& brush) noexcept
        {
            using btl::hash_append;
            if (auto const* color = std::get_if<Color>(&brush.content_))
                hash_append(h, *color);
            else
                hash_append(h, brush.getEffect()->getPasses().size());
        }

    private:
        std::variant<Color, Effect> content_;
    };

    AVG_EXPORT std::ostream& operator<<(std::ostream& stream, Brush const& b);

    /**
     * @brief True when lerp() interpolates between a and b: both are
     * colours, or both are effects of the same kind (isSameKind).
     */
    AVG_EXPORT bool isSameKind(Brush const& a, Brush const& b);
} // namespace avg
