#pragma once

#include <windows.h>

#include "gldispatchedcontext.h"

#include "dispatcher.h"

#include <GL/gl.h>
#include <GL/wglext.h>

namespace ase
{
    class WglPlatform;

    class WglDispatchedContext : public GlDispatchedContext
    {
    public:
        WglDispatchedContext(WglPlatform& platform);

        HGLRC getWglContext() const;

    private:
        WglPlatform& platform_;
        HGLRC context_;
    };
} // namespace ase

