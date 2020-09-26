#pragma once

#include "rendercontext.h"
#include "platform.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class GlRenderContext;

    /**
     * @brief Abstract base class for all OpenGl platforms
     */
    class ASE_EXPORT GlPlatform : public Platform
    {
    public:
        GlPlatform();
        virtual ~GlPlatform();
    };
}

