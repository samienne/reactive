#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <memory>

namespace ase
{
    class RenderContext;
    class Window;

    class ASE_EXPORT PlatformImpl
    {
    public:
        virtual ~PlatformImpl() = default;

        virtual Window makeWindow(Vector2i size) = 0;
        virtual void handleEvents() = 0;
        virtual RenderContext makeRenderContext() = 0;
    };
}

