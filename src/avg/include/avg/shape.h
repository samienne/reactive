#pragma once

#include "pen.h"
#include "brush.h"
#include "path.h"
#include "avgvisibility.h"
#include "operationtype.h"

#include <pmr/memory_resource.h>
#include <pmr/heap.h>

#include <variant>

namespace avg
{
    class Drawing;
    class Region;

    class AVG_EXPORT Shape final
    {
    public:
        Shape(Path path);
        Shape(Shape const&) = default;
        Shape(Shape&&) noexcept = default;

        pmr::memory_resource* getResource() const;

        Shape& operator=(Shape const&) = default;
        Shape& operator=(Shape&&) noexcept = default;

        Shape intersect(Shape&& rhs) &&;
        Shape add(Shape&& rhs) &&;
        Shape subtract(Shape&& rhs) &&;

        Drawing stroke(Pen const& pen) &&;
        Drawing fill(Brush const& brush) &&;
        Drawing fillAndStroke(std::optional<Brush> const& brush,
                std::optional<Pen> const& pen) &&;

        Shape strokeToShape(Pen const& pen) &&;

        Shape transform(Transform t) &&;

        Rect getControlBb() const;

        Shape operator*(float scale) const &;
        Shape operator*(float scale) &&;

        Shape& operator*=(float scale);
        Shape& operator+=(Shape&& rhs);

        friend Shape operator*(Transform const& t, Shape const& rhs);

        Shape with_resource(pmr::memory_resource* memory) const&;
        Shape with_resource(pmr::memory_resource* memory) &&;

        Region strokeToRegion(pmr::memory_resource* memory,
                Pen const& pen, Vector2f pointSize) const;
        Region fillToRegion(pmr::memory_resource* memory,
                Vector2f pointSize) const;

    public:
        struct SubElement;

        struct Operation
        {
            OperationType type;
            pmr::vector<SubElement> elements;
        };

        struct StrokeToShape
        {
            pmr::heap<SubElement> subElement;
            Pen pen;
        };

        using Element = std::variant<Path, Operation, StrokeToShape>;

        struct SubElement
        {
            Transform transform;
            Element element;
        };

    public:
        Shape(pmr::memory_resource* memory, SubElement&& root);

    private:
        pmr::memory_resource* memory_ = nullptr;
        SubElement root_;
    };
}

