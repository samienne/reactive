#pragma once

#include "glplatform.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <windows.h>

#include <GL/gl.h>
#include <GL/wglext.h>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class WindowImpl;

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
        void registerRenderWindow(std::weak_ptr<WindowImpl> window) override;
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

        // Every window this platform makes, real or offscreen; the run loop
        // draws them all from here. Real windows are also in the HWND map (used
        // for message routing), which an offscreen window has no place in.
        std::vector<std::weak_ptr<WindowImpl>> renderWindows_;
    };

} // namespace ase

