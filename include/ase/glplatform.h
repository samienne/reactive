#pragma once

#include "rendercontext.h"
#include "platform.h"

#include <btl/visibility.h>

#include <memory>

namespace ase
{
    class GlRenderContext;

    /**
     * @brief Abstract base class for all OpenGl platforms
     */
    class BTL_VISIBLE GlPlatform : public Platform
    {
    public:
        GlPlatform();
        virtual ~GlPlatform();
    };
}

