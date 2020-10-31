#pragma once


#include "gldispatchedcontext.h"

#include "dispatcher.h"

#include "asevisibility.h"

#include "systemgl.h"

#include <windows.h>

namespace ase
{
    class WglPlatform;

    class WglDispatchedContext : public GlDispatchedContext
    {
    public:
        WglDispatchedContext(WglPlatform& platform, HGLRC context);

        HGLRC getWglContext() const;

    private:
        WglPlatform& platform_;
        HGLRC context_;
    };
} // namespace ase

