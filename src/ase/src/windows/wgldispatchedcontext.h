#pragma once


#include "gldispatchedcontext.h"

#include "dispatcher.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <GL/wglext.h>

#include <windows.h>

namespace ase
{
    class WglPlatform;
    class WglWindow;

    class WglDispatchedContext : public GlDispatchedContext
    {
    public:
        WglDispatchedContext(WglPlatform& platform, HGLRC context);

        HGLRC getWglContext() const;
        void makeCurrent(Dispatched, WglWindow const& window);

    private:
        WglPlatform& platform_;
        HGLRC context_;
        HDC hdc_;
        PFNWGLSWAPINTERVALEXTPROC wglSwapInterval_ = nullptr;
    };
} // namespace ase

