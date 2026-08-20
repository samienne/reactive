#pragma once

#include "rendercontext.h"
#include "platformbase.h"

#include "asevisibility.h"

#include <memory>

namespace ase
{
    class GlRenderContext;

    /**
     * @brief Abstract base class for all OpenGl platforms
     */
    class ASE_EXPORT GlPlatform : public PlatformBase
    {
    public:
        explicit GlPlatform(btl::RunLoop& loop);
        virtual ~GlPlatform();

        virtual bool isBackgroundQueueEnabled() const;
    };
}

