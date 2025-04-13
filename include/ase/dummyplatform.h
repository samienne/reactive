#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT DummyPlatform : public PlatformImpl
    {
    public:
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;
        void run(RenderContext& renderContext,
                std::function<bool(Frame const&)> frameCallback) override;
        void requestFrame() override;
    };

} // namespace ase

