#pragma once

#include "brush.h"
#include "transform.h"
#include "jointype.h"
#include "endtype.h"

#include <btl/hash.h>
#include <btl/visibility.h>

namespace avg
{
    class BTL_VISIBLE Pen final
    {
    public:
        Pen(Brush brush = Brush(), float width = 1.0f,
                JoinType join = JOIN_ROUND, EndType end = END_OPENROUND);

        Pen(Pen const&) = default;
        Pen(Pen&&) = default;

        ~Pen();

        Pen& operator=(Pen const&) = default;
        Pen& operator=(Pen&&) = default;

        Pen operator*(float scale) const noexcept;
        Pen& operator*=(float scale) noexcept;

        Brush const& getBrush() const noexcept;
        float getWidth() const noexcept;
        JoinType getJoinType() const noexcept;
        EndType getEndType() const noexcept;

        bool operator==(Pen const& rhs) const noexcept;
        bool operator!=(Pen const& rhs) const noexcept;
        bool operator<(Pen const& rhs) const noexcept;
        bool operator>(Pen const& rhs) const noexcept;

        friend std::ostream& operator<<(std::ostream& stream, Pen const& p)
        {
            return stream << "Pen{" << "brush: " << p.brush_
                << "width: " << p.width_ << "}";
        }

        friend Pen operator*(Transform const& t, Pen const& p) noexcept
        {
            return Pen(t * p.brush_, t.getScale() * p.width_);
        }

        template <class THash>
        friend void hash_append(THash& h, Pen const& pen) noexcept
        {
            using btl::hash_append;
            hash_append(h, pen.brush_);
            hash_append(h, pen.width_);
        }

    private:
        Brush brush_;
        JoinType join_;
        EndType end_;
        float width_;
    };
}

