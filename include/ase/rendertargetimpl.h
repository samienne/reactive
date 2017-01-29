#pragma once

#include "vector.h"

namespace ase
{
    class RenderContextImpl;
    struct Dispatched;

    class RenderTargetImpl
    {
    public:
        virtual ~RenderTargetImpl() = default;
        virtual void makeCurrent(Dispatched,
                RenderContextImpl& renderContext) const = 0;
        virtual bool isComplete() const = 0;
        virtual Vector2i getResolution() const = 0;
    };
}

