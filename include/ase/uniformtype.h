#pragma once

#include <cassert>
#include <cstring>
#include <stdint.h>

namespace ase
{
    enum class UniformType
    {
        uniform1fv,
        uniform2fv,
        uniform3fv,
        uniform4fv,
        uniform1iv,
        uniform2iv,
        uniform3iv,
        uniform4iv,
        uniform1uiv,
        uniform2uiv,
        uniform3uiv,
        uniform4uiv,
        uniformMatrix4fv
    };

    inline size_t getUniformSize(UniformType type, uint16_t count)
    {
        switch (type)
        {
        case UniformType::uniform1fv:
        case UniformType::uniform1iv:
        case UniformType::uniform1uiv:
            return count * 1 * 4;
        case UniformType::uniform2fv:
        case UniformType::uniform2iv:
        case UniformType::uniform2uiv:
            return count * 2 * 4;
        case UniformType::uniform3fv:
        case UniformType::uniform3iv:
        case UniformType::uniform3uiv:
            return count * 3 * 4;
        case UniformType::uniform4fv:
        case UniformType::uniform4iv:
        case UniformType::uniform4uiv:
            return count * 4 * 4;
        case UniformType::uniformMatrix4fv:
            return count * 16 * 4;
        }

        assert(false && "Unknown UniformType");
        return 0;
    }
} // ase

