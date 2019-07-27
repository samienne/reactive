#pragma once

#include "vector.h"
#include "avgvisibility.h"

#include <pmr/vector.h>
#include <pmr/memory_resource.h>

#include <vector>

namespace avg
{
    class AVG_EXPORT SimplePolygon
    {
    public:
        SimplePolygon(pmr::vector<Vector2i>&& vertices);
        ~SimplePolygon();

        pmr::vector<Vector2i> const& getVertices() const;

    private:
        pmr::vector<Vector2i> vertices_;
    };
}

