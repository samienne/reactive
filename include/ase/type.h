#pragma once

#include <cstring>

namespace ase
{
    enum Type
    {
        TypeFloat,
        TypeInt,
        TypeUInt
    };

    inline size_t typeSize(Type type)
    {
        switch (type)
        {
            case TypeFloat:
                return 4;
            case TypeInt:
                return 4;
            case TypeUInt:
                return 2;
            default:
                return 0;
        }
    }

}

