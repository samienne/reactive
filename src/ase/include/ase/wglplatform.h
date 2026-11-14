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
    class WindowBase;

    class ASE_EXPORT WglPlatform : public GlPlatform
    {
    public:
        explicit WglPlatform(btl::RunLoop& loop);
        virtual ~WglPlatform();

        WglPlatform(WglPlatform const&) = delete;
        WglPlatform& operator=(WglPlatform const&) = delete;

        bool isBackgroundQueueEnabled() const override;

        HGLRC createRawContext(int major, int minor);
        HDC getDummyDc() const;
        PIXELFORMATDESCRIPTOR getPixelFormatDescriptor() const;
        void setPixelFormat(HDC dc) const;

        static std::string getLastErrorString();

        // From PlatformImpl
        Window makeWindow(RenderContext& context, Vector2i size,
                bool headless) override;
        RenderContext makeRenderContext() override;

    protected:
        void handleEvents() override;
        btl::NativeHandle wakeSource() override;
        std::vector<std::weak_ptr<WindowBase>>& getRenderWindows() override;

    private:
        HGLRC createContext();

        HWND dummyWindow_ = nullptr;
        HGLRC dummyContext_ = nullptr;
        HDC dummyDc_ = nullptr;
        int pixelFormat_ = 0;
        bool gl3Enabled_ = true;
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_ = nullptr;

        // Every window this platform makes, real or offscreen; the run loop
        // draws them all from here. Real windows are also in the HWND map (used
        // for message routing), which an offscreen window has no place in.
        std::vector<std::weak_ptr<WindowBase>> renderWindows_;
    };

} // namespace ase

