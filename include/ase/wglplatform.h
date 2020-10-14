#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

#include <windows.h>

#include <GL/gl.h>

#include "wglext.h"

namespace ase
{
    class ASE_EXPORT WglPlatform : public PlatformImpl
    {
    public:
        WglPlatform();

        WglPlatform(WglPlatform const&) = delete;
        WglPlatform& operator=(WglPlatform const&) = delete;

        HGLRC createRawContext(HDC dc);

    private:
        // From PlatformImpl
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

    private:
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_ = nullptr;
    };

} // namespace ase

