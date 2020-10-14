#include "gltype.h"

#include "systemgl.h"

#include <sstream>

namespace ase
{

std::string glTypeToString(GLenum type)
{
    switch (type)
    {
    case GL_FLOAT:
        return "float";
    case GL_FLOAT_VEC2:
        return "vec2";
    case GL_FLOAT_VEC3:
        return "vec3";
    case GL_FLOAT_VEC4:
        return "vec4";
    case GL_INT:
        return "int";
    case GL_INT_VEC2:
        return "ivec2";
    case GL_INT_VEC3:
        return "ivec3";
    case GL_INT_VEC4:
        return "ivec4";
    case GL_BOOL:
        return "bool";
    case GL_BOOL_VEC2:
        return "bvec2";
    case GL_BOOL_VEC3:
        return "bvec3";
    case GL_BOOL_VEC4:
        return "bvec4";
    case GL_FLOAT_MAT2:
        return "mat2";
    case GL_FLOAT_MAT3:
        return "mat3";
    case GL_FLOAT_MAT4:
        return "mat4";
    case GL_SAMPLER_2D:
        return "sampler2D";
    case GL_SAMPLER_CUBE:
        return "samplerCube";
    default:
        std::ostringstream ss;
        ss << "unknown_type(0x" << std::hex << type << ")";
        return ss.str();
    };
}

GLenum typeToGl(Type type)
{
    switch (type)
    {
    case TypeFloat:
        return GL_FLOAT;
    case TypeInt:
        return GL_INT;
    case TypeUInt:
        return GL_UNSIGNED_SHORT;
    default:
        return 0;
    }
}

GLenum primitiveToGl(PrimitiveType type)
{
    switch (type)
    {
    case PrimitiveTriangle:
        return GL_TRIANGLES;
    case PrimitiveTriangleFan:
        return GL_TRIANGLE_FAN;
    case PrimitiveTriangleStrip:
        return GL_TRIANGLE_STRIP;
    case PrimitiveLine:
        return GL_LINES;
    case PrimitivePoint:
        return GL_POINTS;
    default:
        return 0;
    }
}

} // namespace

