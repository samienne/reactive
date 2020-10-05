#pragma once

#include "rendercontext.h"
#include "platformimpl.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class GlRenderContext;

    /**
     * @brief Abstract base class for all OpenGl platforms
     */
    class ASE_EXPORT GlPlatform : public PlatformImpl
    {
    public:
        GlPlatform();
        virtual ~GlPlatform();
    };
}

