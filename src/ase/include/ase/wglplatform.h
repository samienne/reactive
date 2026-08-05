#pragma once

#include "glplatform.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <atomic>
#include <functional>

namespace ase
{
    class ASE_EXPORT WglPlatform : public GlPlatform
    {
    public:
        WglPlatform();
        virtual ~WglPlatform();

        WglPlatform(WglPlatform const&) = delete;
        WglPlatform& operator=(WglPlatform const&) = delete;

        bool isBackgroundQueueEnabled() const override;

        HGLRC createRawContext(int minor, int major);
        HDC getDummyDc() const;
        PIXELFORMATDESCRIPTOR getPixelFormatDescriptor() const;

        static std::string getLastErrorString();

        // From PlatformImpl
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) override;
        void requestFrame() override;

    private:
        HWND dummyWindow_ = nullptr;
        HGLRC dummyContext_ = nullptr;
        HDC dummyDc_ = nullptr;
        PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB_ = nullptr;

        // Set to true by requestFrame() (possibly off-thread) to coalesce a
        // burst of wake requests into a single posted task.
        std::atomic<bool> wakePosted_ = false;

        // Installed by run() so requestFrame(), which cannot see run()'s local
        // tick, can schedule one while the loop is active; cleared at run() exit.
        std::function<void(btl::RunLoop::Controller&)> scheduleTick_;
    };

} // namespace ase

