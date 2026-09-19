#pragma once

#include "platformbase.h"

#include "asevisibility.h"

#include <btl/runloop.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace ase
{
    class Platform;
    class WindowBase;

    class ASE_EXPORT DummyPlatform : public PlatformBase
    {
    public:
        explicit DummyPlatform(btl::RunLoop& loop);

        Window makeWindow(RenderContext& context, Vector2i size,
                bool headless) override;
        RenderContext makeRenderContext() override;

    protected:
        void handleEvents() override;
        RunConfig runConfig() override;
        std::vector<std::weak_ptr<WindowBase>>& getRenderWindows() override;

    private:
        std::vector<std::weak_ptr<WindowBase>> renderWindows_;
    };

    /**
     * @brief Construct a headless platform explicitly, independent of the
     * build's default backend.
     *
     * `makeDefaultPlatform()` selects the OS backend; this always gives the
     * dummy one. `loop` is injected and must outlive the returned platform.
     */
    ASE_EXPORT Platform makeDummyPlatform(btl::RunLoop& loop);

} // namespace ase

