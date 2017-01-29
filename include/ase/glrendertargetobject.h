#pragma once

#include "rendertargetobjectimpl.h"
#include "dispatcher.h"

#include <map>

namespace ase
{
    class GlPlatform;
    class Texture;
    class RenderContext;
    class RenderQueue;

    class GlRenderTargetObject : public RenderTargetObjectImpl
    {
    public:
        GlRenderTargetObject(RenderContext& context);
        GlRenderTargetObject(GlRenderTargetObject const& rhs) = default;
        GlRenderTargetObject(GlRenderTargetObject&& rhs) = default;
        ~GlRenderTargetObject();

        GlRenderTargetObject& operator=(
                GlRenderTargetObject const& rhs) = default;
        GlRenderTargetObject& operator=(GlRenderTargetObject&& rhs) = default;

        // From RenderTargetObjectImpl
        void setColorTarget(size_t index, Texture& texture) override;
        std::shared_ptr<RenderTargetObjectImpl> clone() const override;

        // From RenderTargetImpl
        void makeCurrent(Dispatched,
                RenderContextImpl& renderContext) const override;
        bool isComplete() const override;
        void clear() override;
        Vector2i getResolution() const override;

    private:
        GlPlatform* platform_;
        mutable bool dirty_;
        std::map<size_t, Texture> colorTextures_;
    };
}

