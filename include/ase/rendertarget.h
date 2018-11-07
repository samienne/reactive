#pragma once

//#include "rendercommand.h"

#include "vector.h"

#include <btl/visibility.h>

#include <memory>
#include <vector>

namespace ase
{
    class RenderContext;
    class RenderTargetImpl;
    class Texture;
    class DepthBuffer;
    class Platform;

    /**
     * @brief RenderTarget specifies output of rendering.
     */
    class BTL_VISIBLE RenderTarget
    {
    public:
        RenderTarget();
        RenderTarget(std::shared_ptr<RenderTargetImpl>&& deferred);
        RenderTarget(RenderTarget const& rhs) = default;
        RenderTarget(RenderTarget&& rhs) = default;
        ~RenderTarget();

        RenderTarget& operator=(RenderTarget const& rhs) = default;
        RenderTarget& operator=(RenderTarget&& rhs) = default;

        bool operator==(RenderTarget const& rhs) const;
        bool operator!=(RenderTarget const& rhs) const;
        bool operator<(RenderTarget const& rhs) const;

        operator bool() const;
        bool isComplete() const;

        Vector2i getResolution() const;

        //void push(RenderCommand&& command);
        //void submitSolid(RenderContext& queue);
        //void submitAll(RenderContext& queue);

        template <class T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    protected:
        std::shared_ptr<RenderTargetImpl> deferred_;
        inline RenderTargetImpl* d() { return deferred_.get(); }
        inline RenderTargetImpl const* d() const { return deferred_.get(); }

    private:
        //std::vector<RenderCommand> blendedCommands_;
        //std::vector<RenderCommand> solidCommands_;
    };
}

