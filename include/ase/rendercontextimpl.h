#pragma once

#include <btl/visibility.h>

#include <vector>

namespace ase
{
    class Window;
    class RenderCommand;
    class RenderTarget;

    class BTL_VISIBLE RenderContextImpl
    {
    public:
        virtual ~RenderContextImpl() = default;

        virtual void submit(RenderTarget& target,
                std::vector<RenderCommand>&& commands) = 0;
        virtual void flush() = 0;
        virtual void finish() = 0;
        virtual void present(Window& window) = 0;
    };
}

