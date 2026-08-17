#pragma once

#include "glplatform.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class WindowImpl;

    class ASE_EXPORT WglPlatform : public GlPlatform
    {
    public:
        explicit WglPlatform(btl::RunLoop& loop);
        virtual ~WglPlatform();

        WglPlatform(WglPlatform const&) = delete;
        WglPlatform& operator=(WglPlatform const&) = delete;

        bool isBackgroundQueueEnabled() const override;

        HGLRC createRawContext(int minor, int major);
        HDC getDummyDc() const;
        PIXELFORMATDESCRIPTOR getPixelFormatDescriptor() const;

        static std::string getLastErrorString();

        // From PlatformImpl
        Window makeWindow(RenderContext& context, Vector2i size,
                bool headless) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

    protected:
        RunConfig runConfig() override;

    private:
        HWND dummyWindow_ = nullptr;
        HGLRC dummyContext_ = nullptr;
        HDC dummyDc_ = nullptr;
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_ = nullptr;

        // Every window this platform makes, real or offscreen; the run loop
        // draws them all from here. Real windows are also in the HWND map (used
        // for message routing), which an offscreen window has no place in.
        std::vector<std::weak_ptr<WindowImpl>> renderWindows_;
    };

} // namespace ase

