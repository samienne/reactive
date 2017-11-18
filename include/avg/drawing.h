#pragma once

#include "shape.h"
#include "textentry.h"
#include "transform.h"

#include <btl/variant.h>

namespace avg
{
    class Drawing final
    {
    public:
        using Element = btl::variant<Shape, TextEntry>;

        Drawing();
        Drawing(Element const& element);
        Drawing(std::vector<Element> const& elements);
        Drawing(std::vector<Element>&& elements);

        ~Drawing();

        Drawing(Drawing const&) = default;
        Drawing(Drawing&&) noexcept = default;

        Drawing& operator=(Drawing const&) = default;
        Drawing& operator=(Drawing&&) noexcept = default;

        Drawing operator+(Element&& element) const &;
        Drawing operator+(Element&& element) &&;
        Drawing& operator+=(Element&& element);

        Drawing operator+(Drawing const& drawing) const &;
        Drawing operator+(Drawing const& drawing) &&;
        Drawing& operator+=(Drawing const& drawing);

        Drawing operator*(float scale) const &;
        Drawing operator*(float scale) &&;
        Drawing operator+(ase::Vector2f offset) const &;
        Drawing operator+(ase::Vector2f offset) &&;

        bool operator==(Drawing const& rhs) const;
        bool operator!=(Drawing const& rhs) const;
        bool operator<(Drawing const& rhs) const;
        bool operator>(Drawing const& rhs) const;

        std::vector<Element> const& getElements() const;

        Drawing transform(Transform const& t) &&;

        friend Drawing operator*(Transform const& t, Drawing&& drawing);

    private:
        std::vector<Element> elements_;
    };
}

