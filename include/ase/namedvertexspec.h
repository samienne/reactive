#pragma once

#include "type.h"
#include "primitivetype.h"

#include <btl/visibility.h>

#include <string>
#include <vector>

namespace ase
{
    class BTL_VISIBLE NamedVertexSpec
    {
    public:
        struct Spec
        {
            std::string attrib;
            int size;
            Type type;
            bool normalized;
            size_t pointer;

            inline bool operator==(Spec const& rhs) const
            {
                return attrib == rhs.attrib && size == rhs.size
                    && type == rhs.type && normalized == rhs.normalized
                    && pointer == rhs.pointer;
            }

            inline bool operator!=(Spec const& rhs) const
            {
                return attrib != rhs.attrib || size != rhs.size
                    || type != rhs.type || normalized != rhs.normalized
                    || pointer != rhs.pointer;
            }

            inline bool operator<(Spec const& rhs) const
            {
                if (attrib != rhs.attrib)
                    return attrib < rhs.attrib;
                if (size != rhs.size)
                    return size < rhs.size;
                if (type != rhs.type)
                    return type < rhs.type;
                if (normalized != rhs.normalized)
                    return normalized < rhs.normalized;
                return pointer < rhs.pointer;
            }
        };

        NamedVertexSpec();
        NamedVertexSpec(NamedVertexSpec const& rhs) = default;
        NamedVertexSpec(NamedVertexSpec&& rhs) = default;
        NamedVertexSpec(PrimitiveType type, size_t stride = 0);
        ~NamedVertexSpec();

        NamedVertexSpec& operator=(NamedVertexSpec const& rhs) = default;
        NamedVertexSpec& operator=(NamedVertexSpec&& rhs) = default;

        /**
         * @brief Adds a new attribute.
         *
         * @param attrib     The name of the attribute.
         * @param size       The amount of variable of type.
         * @param type       The type of the variable.
         * @param normalized True if the data is to be normalized.
         * @param pointer    Offset to the end of the previous attribute.
         */
        NamedVertexSpec& add(std::string const& attrib, int size, Type type,
                bool normalized, size_t pointer = 0);

        std::vector<Spec> const& getSpecs() const;
        PrimitiveType getPrimitiveType() const;
        size_t getStride() const;

        bool operator==(NamedVertexSpec const& rhs) const;
        bool operator!=(NamedVertexSpec const& rhs) const;

    private:
        std::vector<Spec> specs_;
        PrimitiveType primitiveType_;
        size_t stride_;
        size_t next_;
    };
}

