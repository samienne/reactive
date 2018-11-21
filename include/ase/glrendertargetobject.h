#pragma once

#include "texture.h"

#include "rendertargetobjectimpl.h"
#include "dispatcher.h"

#include <btl/option.h>
#include <btl/visibility.h>

#include <unordered_map>

namespace ase
{
    class GlPlatform;
    class GlRenderContext;
    class Texture;
    class RenderQueue;
    struct GlFunctions;

    class BTL_VISIBLE GlRenderTargetObject : public RenderTargetObjectImpl
    {
    public:
        GlRenderTargetObject(GlRenderContext& context);
        GlRenderTargetObject(GlRenderTargetObject const& rhs) = default;
        GlRenderTargetObject(GlRenderTargetObject&& rhs) = default;
        ~GlRenderTargetObject();

        GlRenderTargetObject& operator=(
                GlRenderTargetObject const& rhs) = default;
        GlRenderTargetObject& operator=(GlRenderTargetObject&& rhs) = default;

        // From RenderTargetObjectImpl
        void setColorTarget(size_t index, btl::option<Texture> texture) override;
        std::shared_ptr<RenderTargetObjectImpl> clone() const override;

        // From RenderTargetImpl
        void makeCurrent(Dispatched,
                RenderContextImpl& renderContext) const override;
        bool isComplete() const override;
        void clear() override;
        Vector2i getResolution() const override;

    private:
        GlRenderContext& context_;
        mutable bool dirty_;
        std::unordered_map<size_t, Texture> colorTextures_;
    };
}

