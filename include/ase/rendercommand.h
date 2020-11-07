#pragma once

#include "drawcommand.h"

#include <variant>

namespace ase
{
    struct ClearCommand
    {
        Framebuffer target;
        float r = 0.0f;
        float g = 0.0f;
        float b = 0.0f;
        float a = 1.0f;
        bool color = true;
        bool depth = true;
        bool stencil = true;
    };

    using RenderCommand = std::variant<DrawCommand, ClearCommand>;

} // namespace ase
