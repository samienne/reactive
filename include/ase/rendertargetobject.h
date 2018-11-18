#pragma once

#include "rendertarget.h"

#include <btl/option.h>
#include <btl/visibility.h>

#include <memory>
#include <map>

namespace ase
{
    class RenderCommand;
    class RenderTargetObjectImpl;
    class Texture;
    class Platform;

    class BTL_VISIBLE RenderTargetObject : public RenderTarget
    {
    public:
        RenderTargetObject(std::shared_ptr<RenderTargetObjectImpl> impl);
        RenderTargetObject(RenderTargetObject const& rhs) = default;
        RenderTargetObject(RenderTargetObject&& rhs) = default;
        ~RenderTargetObject();

        RenderTargetObject& operator=(RenderTargetObject const& rhs) = default;
        RenderTargetObject& operator=(RenderTargetObject&& rhs) = default;

        void setColorTarget(size_t index, btl::option<Texture> texture);
        void clear();

    private:
        std::shared_ptr<RenderTargetObjectImpl> deferred_;
        inline RenderTargetObjectImpl* d()
        {
            return reinterpret_cast<RenderTargetObjectImpl*>(RenderTarget::d());
        }

        inline RenderTargetObjectImpl const* d() const
        {
            return reinterpret_cast<RenderTargetObjectImpl const*>(
                    RenderTarget::d());
        }
    };
}

