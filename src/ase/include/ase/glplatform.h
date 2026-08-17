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
        explicit GlPlatform(btl::RunLoop& loop);
        virtual ~GlPlatform();

        virtual bool isBackgroundQueueEnabled() const;
    };
}

