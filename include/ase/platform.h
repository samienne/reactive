#pragma once

#include "vector.h"
#include "asevisibility.h"

#include <memory>

namespace ase
{
    class Window;
    class RenderContext;
    class PlatformImpl;

    class ASE_EXPORT Platform
    {
    public:
        Platform(std::shared_ptr<PlatformImpl> impl);
        virtual ~Platform();

        Window makeWindow(Vector2i size, float scalingFactor = 1.0f);
        void handleEvents();
        RenderContext makeRenderContext();

        template <typename T>
        T& getImpl()
        {
            return reinterpret_cast<T&>(*d());
        }

        template <typename T>
        T const& getImpl() const
        {
            return reinterpret_cast<T const&>(*d());
        }

    private:
        PlatformImpl* d()
        {
            return deferred_.get();
        }

        PlatformImpl const* d() const
        {
            return deferred_.get();
        }

    private:
        std::shared_ptr<PlatformImpl> deferred_;
    };

    ASE_EXPORT Platform makeDefaultPlatform();
} // ase

