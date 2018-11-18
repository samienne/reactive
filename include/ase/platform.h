#pragma once

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class RenderContext;
    class RenderContextImpl;

    /**
     * @brief Abstract base class for all platforms
     */
    class BTL_VISIBLE Platform
    {
    public:
        virtual ~Platform() = default;

    private:
        friend class RenderContext;

        virtual std::shared_ptr<RenderContextImpl> makeRenderContextImpl() = 0;
    };
} // ase

