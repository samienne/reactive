#pragma once

#include "glplatform.h"

#include "asevisibility.h"

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

namespace ase
{
    class ASE_EXPORT WglPlatform : public GlPlatform
    {
    public:
        WglPlatform();
        virtual ~WglPlatform();

        WglPlatform(WglPlatform const&) = delete;
        WglPlatform& operator=(WglPlatform const&) = delete;

        HGLRC createRawContext(int minor, int major);
        HDC getDummyDc() const;
        PIXELFORMATDESCRIPTOR getPixelFormatDescriptor() const;

        static std::string getLastErrorString();

    private:
        // From PlatformImpl
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

    private:
        HWND dummyWindow_ = nullptr;
        HGLRC dummyContext_ = nullptr;
        HDC dummyDc_ = nullptr;
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_ = nullptr;
    };

} // namespace ase

