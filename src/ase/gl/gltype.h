#pragma once

#include "type.h"
#include "primitivetype.h"

#include <GL/gl.h>

#include <string>

namespace ase
{
    std::string glTypeToString(GLenum type);
    GLenum typeToGl(Type type);
    GLenum primitiveToGl(PrimitiveType type);
}

