#pragma once

#include "platformimpl.h"

#include "asevisibility.h"

#include <windows.h>

namespace ase
{
    class ASE_EXPORT WglPlatform : public PlatformImpl
    {
    public:
        WglPlatform();

        WglPlatform(WglPlatform const&) = delete;
        WglPlatform& operator=(WglPlatform const&) = delete;

    private:
        // From PlatformImpl
        Window makeWindow(Vector2i size) override;
        void handleEvents() override;
        RenderContext makeRenderContext() override;

    private:
    };

} // namespace ase

