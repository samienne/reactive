#pragma once

#include "glxcontext.h"

#include "gldispatchedcontext.h"

#include "dispatcher.h"

namespace ase
{
    class GlxPlatform;

    class GlxDispatchedContext : public GlDispatchedContext
    {
    public:
        GlxDispatchedContext(GlxPlatform& platform);

        GlxContext const& getGlxContext() const;
        GlxContext& getGlxContext();

    private:
        GlxPlatform& platform_;
        GlxContext context_;
    };
} // namespace ase

