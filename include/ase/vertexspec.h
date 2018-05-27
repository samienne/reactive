#pragma once

#include "type.h"
#include "primitivetype.h"

#include <btl/visibility.h>

#include <string>
#include <vector>

namespace ase
{
    class Program;
    class NamedVertexSpec;

    class BTL_VISIBLE VertexSpec
    {
    public:
        struct Spec
        {
            int attribLoc;
            int size;
            Type type;
            bool normalized;
            size_t pointer;

            inline bool operator==(Spec const& rhs) const
            {
                return attribLoc == rhs.attribLoc && size == rhs.size
                    && type == rhs.type && normalized == rhs.normalized
                    && pointer == rhs.pointer;
            }

            inline bool operator!=(Spec const& rhs) const
            {
                return attribLoc != rhs.attribLoc || size != rhs.size
                    || type != rhs.type || normalized != rhs.normalized
                    || pointer != rhs.pointer;
            }

            inline bool operator<(Spec const& rhs) const
            {
                if (attribLoc != rhs.attribLoc)
                    return attribLoc < rhs.attribLoc;
                if (size != rhs.size)
                    return size < rhs.size;
                if (type != rhs.type)
                    return type < rhs.type;
                if (normalized != rhs.normalized)
                    return normalized < rhs.normalized;
                return pointer < rhs.pointer;
            }
        };

        VertexSpec();
        VertexSpec(Program const& program, NamedVertexSpec const& spec);
        VertexSpec(VertexSpec const& rhs) = default;
        VertexSpec(VertexSpec&& rhs) = default;
        ~VertexSpec();

        std::vector<Spec> const& getSpecs() const;
        PrimitiveType getPrimitiveType() const;
        size_t getStride() const;

        VertexSpec& operator=(VertexSpec const& rhs) = default;
        VertexSpec& operator=(VertexSpec&& rhs) = default;

        bool operator==(VertexSpec const& rhs) const;
        bool operator!=(VertexSpec const& rhs) const;
        bool operator<(VertexSpec const& rhs) const;

        bool isEmpty() const;

    private:
        std::vector<Spec> specs_;
        PrimitiveType primitiveType_;
        size_t stride_;
    };
}

