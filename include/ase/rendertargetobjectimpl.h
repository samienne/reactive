#pragma once

#include "rendertargetimpl.h"

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class Texture;

    class BTL_VISIBLE RenderTargetObjectImpl : public RenderTargetImpl
    {
    public:
        virtual ~RenderTargetObjectImpl() = default;

        virtual void setColorTarget(size_t index, Texture& texture) = 0;
        virtual std::shared_ptr<RenderTargetObjectImpl> clone() const = 0;
        virtual void clear() = 0;
    };
}

