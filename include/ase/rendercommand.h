#pragma once

#include "window.h"
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

    struct PresentCommand
    {
        Window window;
    };

    struct FenceCommand
    {
        struct Control
        {
            std::function<void()> completeCb;
        };

        std::shared_ptr<Control> control_;
    };

    using RenderCommand = std::variant<
        DrawCommand,
        ClearCommand,
        PresentCommand,
        FenceCommand
        >;

} // namespace ase
