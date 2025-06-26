#include "glblendmode.h"

namespace ase
{

GLenum blendModeToGl(BlendMode mode)
{
    switch (mode)
    {
        case BlendMode::Zero:
            return GL_ZERO;
        case BlendMode::One:
            return GL_ONE;
        case BlendMode::SrcColor:
            return GL_SRC_COLOR;
        case BlendMode::OneMinusSrcColor:
            return GL_ONE_MINUS_SRC_COLOR;
        case BlendMode::DstColor:
            return GL_DST_COLOR;
        case BlendMode::OneMinusDstColor:
            return GL_ONE_MINUS_DST_COLOR;
        case BlendMode::SrcAlpha:
            return GL_SRC_ALPHA;
        case BlendMode::OneMinusSrcAlpha:
            return GL_ONE_MINUS_SRC_ALPHA;
        case BlendMode::DstAlpha:
            return GL_DST_ALPHA;
        case BlendMode::OneMinusDstAlpha:
            return GL_ONE_MINUS_DST_ALPHA;
        case BlendMode::ConstantColor:
            return GL_CONSTANT_COLOR;
        case BlendMode::OneMinusConstantColor:
            return GL_ONE_MINUS_CONSTANT_COLOR;
        case BlendMode::ConstantAlpha:
            return GL_CONSTANT_ALPHA;
        case BlendMode::OneMinusConstantAlpha:
            return GL_ONE_MINUS_CONSTANT_ALPHA;
        default:
            return 0;
    }
}

} // namespace

