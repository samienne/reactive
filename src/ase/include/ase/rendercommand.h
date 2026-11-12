#pragma once

#include "window.h"
#include "drawcommand.h"

#include <btl/future/promise.h>

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

    struct FenceCommand
    {
        btl::future::Promise<> promise;
    };

    /**
     * @brief Ends the run of draws before it; otherwise does nothing.
     *
     * Draws between two non-draw commands may be reordered against each
     * other. A barrier separates draws that must stay in order, such as
     * a draw into a texture and a later draw that samples it.
     */
    struct BarrierCommand
    {
    };

    struct BufferUploadCommand
    {
        std::variant<VertexBuffer, IndexBuffer, UniformBuffer> target;
        Buffer data;
        Usage usage;
    };

    struct TextureUploadCommand
    {
        Texture target;
        Buffer data;
        Vector2i size;
        Format format;
    };

    struct TextureSubUploadCommand
    {
        Texture target;
        Buffer data;
        Vector2i offset;
        Vector2i size;
    };

    using RenderCommand = std::variant<
        DrawCommand,
        ClearCommand,
        FenceCommand,
        BarrierCommand,
        BufferUploadCommand,
        TextureUploadCommand,
        TextureSubUploadCommand
        >;

} // namespace ase
