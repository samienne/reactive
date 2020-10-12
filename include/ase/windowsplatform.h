#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

namespace ase
{
    class ASE_EXPORT WindowsPlatform : public PlatformImpl
    {
    public:
    private:
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;
    };

} // namespace ase

